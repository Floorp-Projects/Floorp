#!/bin/sh

for task in "$@"; do
  echo "deb [trusted=yes] https://queue.taskcluster.net/v1/task/$task/artifacts/public/build/ debian/" > "/etc/apt/sources.list.d/99$task.list"
done
