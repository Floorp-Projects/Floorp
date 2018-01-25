#!/bin/sh

for task in "$@"; do
  echo "deb [trusted=yes] https://queue.taskcluster.net/v1/task/$task/runs/0/artifacts/public/build/ debian/"
done > /etc/apt/sources.list.d/99packages.list
