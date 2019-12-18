# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER    Tarek Ziade <tarek@mozilla.com>

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

RUN apt-get -y update && \
    apt-get install -y python3.7

CMD ["/bin/bash", "--login"]

