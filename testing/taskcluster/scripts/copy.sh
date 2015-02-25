#! /bin/bash -ex

# This script copies the contents of the "scripts" folder into a docker
# container using tar/untar the container id must be passed.

DIRNAME=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
docker exec $1 mkdir -p $2
cd $DIRNAME
tar -cv * | docker exec -i $1 tar -x -C $2
