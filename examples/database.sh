#!/bin/bash
# Example: Running a database container

sudo cruntime run \
  --name postgres-db \
  --memory 2G \
  --cpus 2.0 \
  -v /data/postgres:/var/lib/postgresql/data \
  -e POSTGRES_PASSWORD=secretpassword \
  -e POSTGRES_DB=myapp \
  -p 5432:5432/tcp \
  --detach \
  alpine \
  /usr/local/bin/postgres
