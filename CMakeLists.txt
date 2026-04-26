cmake_minimum_required(VERSION 3.10)
project(cruntime C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
add_compile_definitions(_GNU_SOURCE)
add_compile_options(-Wall -Wextra -O2)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.c
    src/core/runtime.c
    src/namespace/namespace.c
    src/cgroup/cgroup.c
    src/network/network.c
    src/storage/storage.c
    src/utils/utils.c
)

# Create executable
add_executable(cruntime ${SOURCES})

# Installation
install(TARGETS cruntime DESTINATION /usr/local/bin)
install(DIRECTORY DESTINATION /var/run/cruntime)
install(DIRECTORY DESTINATION /var/lib/cruntime/state)
install(DIRECTORY DESTINATION /var/lib/cruntime/images)
install(DIRECTORY DESTINATION /var/run/cruntime/netns)

# Custom targets
add_custom_target(check
    COMMAND bash -c "which ip || echo 'Warning: ip command not found'"
    COMMAND bash -c "which iptables || echo 'Warning: iptables not found'"
    COMMAND bash -c "which nsenter || echo 'Warning: nsenter not found'"
    COMMAND bash -c "test -d /sys/fs/cgroup || echo 'Warning: cgroups v2 not available'"
    COMMENT "Checking for required tools"
)

# Debug build
set(CMAKE_BUILD_TYPE_INIT "Release")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -DDEBUG)
endif()

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
