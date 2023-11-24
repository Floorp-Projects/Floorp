#!/bin/sh

set -e

for task in "$@"; do
  echo "removing package source /etc/apt/sources.list.d/99$task.list"
  rm -f "/etc/apt/sources.list.d/99$task.list"
done
apt-get update
