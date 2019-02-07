#!/bin/sh

TASKCLUSTER_ROOT_URL=$1
shift

# duplicate the functionality of taskcluster-lib-urls, but in bash..
if [ "$TASKCLUSTER_ROOT_URL" = "https://taskcluster.net" ]; then
    queue_base='https://queue.taskcluster.net/v1'
else
    queue_base="$TASKCLUSTER_ROOT_URL/api/queue/v1"
fi


for task in "$@"; do
  echo "adding package source $queue_base/task/$task/artifacts/public/build/"
  echo "deb [trusted=yes] $queue_base/task/$task/artifacts/public/build/ debian/" > "/etc/apt/sources.list.d/99$task.list"
done
