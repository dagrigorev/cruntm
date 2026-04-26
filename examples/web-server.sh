#!/bin/bash
# Example: Running a web server container with CRuntime

sudo cruntime run \
  --name my-webserver \
  --hostname webserver01 \
  --memory 1G \
  --cpus 1.5 \
  --network production \
  -p 8080:80/tcp \
  -p 8443:443/tcp \
  -v /data/www:/var/www \
  -v /data/config:/etc/nginx \
  -e ENVIRONMENT=production \
  -e LOG_LEVEL=info \
  --workdir /var/www \
  --detach \
  alpine \
  nginx -g "daemon off;"
