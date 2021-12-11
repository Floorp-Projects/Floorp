#!/bin/sh

TASKCLUSTER_ROOT_URL=$1
shift

# duplicate the functionality of taskcluster-lib-urls, but in bash..
queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"


for task in "$@"; do
  echo "adding package source $queue_base/task/$task/artifacts/public/build/"
  echo "deb [trusted=yes] $queue_base/task/$task/artifacts/public/build/ apt/" > "/etc/apt/sources.list.d/99$task.list"
done
