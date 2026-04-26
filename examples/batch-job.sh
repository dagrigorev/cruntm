#!/bin/bash
# Example: Running a batch processing job

sudo cruntime run \
  --name batch-processor \
  --memory 4G \
  --cpus 4.0 \
  -v /data/input:/input \
  -v /data/output:/output \
  -e BATCH_SIZE=1000 \
  -e THREADS=4 \
  --rm \
  alpine \
  /app/process-data.sh
