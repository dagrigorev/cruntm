#!/bin/bash
# Example: Development environment container

sudo cruntime run \
  --name dev-env \
  -v $(pwd):/workspace \
  -v ~/.gitconfig:/root/.gitconfig \
  -e EDITOR=vim \
  --workdir /workspace \
  alpine \
  /bin/sh
