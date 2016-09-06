set -ex

if [ ! -z "$USE_DOCKER" ]
then
    ls .
    tag="port-of-rust/$TARGET/latest"
    docker build https://github.com/alexcrichton/port-of-rust.git -f "$TARGET/Dockerfile" -t $tag
    docker run\
           -e USER="$USER"\
           -e TARGET="$TARGET"\
           -e USE_DOCKER=1\
           -e NAME="$NAME"\
           -e TRAVIS_TAG="$TRAVIS_TAG"\
           -e USER_ID=$(id -u)\
           -e GROUP_ID=$(id -g)\
           -v $PWD:/mnt/host\
           -i $tag\
           bash -s -- < build.sh
else
    bash build.sh
fi
