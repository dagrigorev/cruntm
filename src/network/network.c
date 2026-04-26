#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <fcntl.h>
#include "../include/cruntime.h"

/* Execute network command */
static int exec_cmd(const char *cmd) {
    cr_log(LOG_DEBUG, "Executing: %s", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        cr_log(LOG_ERROR, "Command failed: %s (exit code: %d)", cmd, ret);
        return CR_ERROR;
    }
    return CR_OK;
}

/* Create bridge network */
int network_create_bridge(const char *bridge_name, const char *subnet) {
    char cmd[512];
    
    /* Check if bridge already exists */
    snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", bridge_name);
    if (system(cmd) == 0) {
        cr_log(LOG_INFO, "Bridge %s already exists", bridge_name);
        return CR_OK;
    }
    
    /* Create bridge */
    snprintf(cmd, sizeof(cmd), "ip link add name %s type bridge", bridge_name);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Parse subnet to get network address and mask */
    char network[64], *slash;
    strncpy(network, subnet, sizeof(network));
    slash = strchr(network, '/');
    if (!slash) {
        cr_log(LOG_ERROR, "Invalid subnet format: %s", subnet);
        return CR_EINVAL;
    }
    
    /* Calculate gateway (first IP in subnet) */
    char gateway[64];
    *slash = '\0';
    struct in_addr addr;
    inet_pton(AF_INET, network, &addr);
    addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
    inet_ntop(AF_INET, &addr, gateway, sizeof(gateway));
    
    /* Assign IP to bridge */
    snprintf(cmd, sizeof(cmd), "ip addr add %s/%s dev %s", 
             gateway, slash + 1, bridge_name);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Bring bridge up */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", bridge_name);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Enable IP forwarding */
    exec_cmd("sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1");
    
    /* Setup NAT */
    snprintf(cmd, sizeof(cmd), 
             "iptables -t nat -A POSTROUTING -s %s ! -o %s -j MASQUERADE",
             subnet, bridge_name);
    exec_cmd(cmd);
    
    /* Allow forwarding */
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -i %s -o %s -j ACCEPT",
             bridge_name, bridge_name);
    exec_cmd(cmd);
    
    snprintf(cmd, sizeof(cmd),
             "iptables -A FORWARD -o %s -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT",
             bridge_name);
    exec_cmd(cmd);
    
    cr_log(LOG_INFO, "Created bridge %s with subnet %s", bridge_name, subnet);
    return CR_OK;
}

/* Delete bridge network */
int network_delete_bridge(const char *bridge_name) {
    char cmd[512];
    
    /* Bring bridge down */
    snprintf(cmd, sizeof(cmd), "ip link set %s down", bridge_name);
    exec_cmd(cmd);
    
    /* Delete bridge */
    snprintf(cmd, sizeof(cmd), "ip link delete %s type bridge", bridge_name);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    cr_log(LOG_INFO, "Deleted bridge %s", bridge_name);
    return CR_OK;
}

/* Setup veth pair for container */
int network_setup_veth(const char *container_id, const char *bridge_name,
                       const char *ip_address, const char *gateway,
                       int container_pid) {
    char cmd[512];
    char veth_host[32], veth_container[32];
    
    /* Generate veth interface names */
    snprintf(veth_host, sizeof(veth_host), "veth%.*s", 8, container_id);
    snprintf(veth_container, sizeof(veth_container), "eth0");
    
    /* Create veth pair */
    snprintf(cmd, sizeof(cmd),
             "ip link add %s type veth peer name %s",
             veth_host, veth_container);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Attach host end to bridge */
    snprintf(cmd, sizeof(cmd), "ip link set %s master %s", veth_host, bridge_name);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Bring host end up */
    snprintf(cmd, sizeof(cmd), "ip link set %s up", veth_host);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Move container end to container's network namespace */
    snprintf(cmd, sizeof(cmd), "ip link set %s netns %d", veth_container, container_pid);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Configure container's network (executed in container namespace) */
    snprintf(cmd, sizeof(cmd),
             "nsenter -t %d -n ip addr add %s dev %s",
             container_pid, ip_address, veth_container);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Bring container interface up */
    snprintf(cmd, sizeof(cmd),
             "nsenter -t %d -n ip link set %s up",
             container_pid, veth_container);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Set default route in container */
    snprintf(cmd, sizeof(cmd),
             "nsenter -t %d -n ip route add default via %s",
             container_pid, gateway);
    if (exec_cmd(cmd) != CR_OK) return CR_ERROR;
    
    /* Setup loopback in container */
    snprintf(cmd, sizeof(cmd),
             "nsenter -t %d -n ip link set lo up",
             container_pid);
    exec_cmd(cmd);
    
    cr_log(LOG_INFO, "Setup veth pair for container %s (IP: %s)", 
           container_id, ip_address);
    return CR_OK;
}

/* Setup port forwarding */
int network_setup_port_forwarding(const char *container_ip, 
                                   port_mapping_t *ports, int num_ports) {
    char cmd[512];
    
    for (int i = 0; i < num_ports; i++) {
        port_mapping_t *pm = &ports[i];
        
        /* Add DNAT rule to forward host port to container */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t nat -A PREROUTING -p %s --dport %d -j DNAT --to-destination %s:%d",
                 pm->protocol, pm->host_port, container_ip, pm->container_port);
        
        if (exec_cmd(cmd) != CR_OK) {
            cr_log(LOG_WARN, "Failed to setup port forwarding %d->%d",
                   pm->host_port, pm->container_port);
            continue;
        }
        
        /* Allow forwarding */
        snprintf(cmd, sizeof(cmd),
                 "iptables -A FORWARD -p %s -d %s --dport %d -j ACCEPT",
                 pm->protocol, container_ip, pm->container_port);
        exec_cmd(cmd);
        
        cr_log(LOG_INFO, "Forwarded port %s/%d -> %d",
               pm->protocol, pm->host_port, pm->container_port);
    }
    
    return CR_OK;
}

/* Remove port forwarding */
int network_remove_port_forwarding(const char *container_ip,
                                    port_mapping_t *ports, int num_ports) {
    char cmd[512];
    
    for (int i = 0; i < num_ports; i++) {
        port_mapping_t *pm = &ports[i];
        
        /* Remove DNAT rule */
        snprintf(cmd, sizeof(cmd),
                 "iptables -t nat -D PREROUTING -p %s --dport %d -j DNAT --to-destination %s:%d",
                 pm->protocol, pm->host_port, container_ip, pm->container_port);
        exec_cmd(cmd);
        
        /* Remove forward rule */
        snprintf(cmd, sizeof(cmd),
                 "iptables -D FORWARD -p %s -d %s --dport %d -j ACCEPT",
                 pm->protocol, container_ip, pm->container_port);
        exec_cmd(cmd);
    }
    
    return CR_OK;
}

/* Allocate IP address for container */
int network_allocate_ip(const char *subnet, char *ip_address, size_t ip_len) {
    /* Simple IP allocation - use container hash to generate IP */
    /* In production, this would use proper IPAM */
    
    char network[64], *slash;
    strncpy(network, subnet, sizeof(network));
    slash = strchr(network, '/');
    if (!slash) return CR_EINVAL;
    
    *slash = '\0';
    struct in_addr addr;
    inet_pton(AF_INET, network, &addr);
    
    /* Generate random offset (2-254) */
    srand(time(NULL) ^ getpid());
    int offset = (rand() % 253) + 2;
    
    addr.s_addr = htonl(ntohl(addr.s_addr) + offset);
    inet_ntop(AF_INET, &addr, ip_address, ip_len);
    
    /* Append CIDR */
    strncat(ip_address, "/", ip_len - strlen(ip_address) - 1);
    strncat(ip_address, slash + 1, ip_len - strlen(ip_address) - 1);
    
    return CR_OK;
}

/* Setup container networking */
int network_setup_container(container_t *container) {
    network_config_t *net = &container->config.network;
    
    /* Create bridge if doesn't exist */
    if (network_create_bridge(net->bridge_name, net->subnet) != CR_OK) {
        return CR_ERROR;
    }
    
    /* Allocate IP if not specified */
    if (strlen(net->ip_address) == 0) {
        if (network_allocate_ip(net->subnet, net->ip_address, 
                               sizeof(net->ip_address)) != CR_OK) {
            return CR_ERROR;
        }
    }
    
    /* Calculate gateway if not specified */
    if (strlen(net->gateway) == 0) {
        char network[64], *slash;
        strncpy(network, net->subnet, sizeof(network));
        slash = strchr(network, '/');
        if (slash) {
            *slash = '\0';
            struct in_addr addr;
            inet_pton(AF_INET, network, &addr);
            addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
            inet_ntop(AF_INET, &addr, net->gateway, sizeof(net->gateway));
        }
    }
    
    /* Setup veth pair */
    if (network_setup_veth(container->config.id, net->bridge_name,
                          net->ip_address, net->gateway,
                          container->init_pid) != CR_OK) {
        return CR_ERROR;
    }
    
    /* Setup port forwarding */
    if (net->num_ports > 0) {
        /* Extract just the IP without CIDR */
        char container_ip[64];
        strncpy(container_ip, net->ip_address, sizeof(container_ip));
        char *slash = strchr(container_ip, '/');
        if (slash) *slash = '\0';
        
        network_setup_port_forwarding(container_ip, net->ports, net->num_ports);
    }
    
    return CR_OK;
}

/* Cleanup container networking */
int network_cleanup_container(container_t *container) {
    char cmd[512];
    char veth_host[32];
    network_config_t *net = &container->config.network;
    
    /* Remove port forwarding */
    if (net->num_ports > 0) {
        char container_ip[64];
        strncpy(container_ip, net->ip_address, sizeof(container_ip));
        char *slash = strchr(container_ip, '/');
        if (slash) *slash = '\0';
        
        network_remove_port_forwarding(container_ip, net->ports, net->num_ports);
    }
    
    /* Delete veth pair */
    snprintf(veth_host, sizeof(veth_host), "veth%.*s", 8, container->config.id);
    snprintf(cmd, sizeof(cmd), "ip link delete %s 2>/dev/null", veth_host);
    exec_cmd(cmd);
    
    return CR_OK;
}
