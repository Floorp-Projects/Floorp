set -ex

if [ ! -z "$DOCKER_IMAGE" ]
then
	tag="$DOCKER_IMAGE/latest"
	docker build $DOCKER_IMAGE -t $tag
	docker run \
	    	-e USER="$USER" \
	    	-e TARGET="$TARGET" \
	    	-e TOOLCHAIN="$TOOLCHAIN" \
	    	-e DOCKER_IMAGE="$DOCKER_IMAGE" \
	    	-e NAME="$NAME" \
	    	-e TRAVIS_TAG="$TRAVIS_TAG" \
	    	-e USER_ID=$(id -u) \
	    	-e GROUP_ID=$(id -g) \
		-v $PWD:/mnt/host \
		-i $tag \
		bash -s -- < build.sh
else
	bash build.sh
fi
