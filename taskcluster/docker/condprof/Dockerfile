FROM $DOCKER_IMAGE_PARENT
MAINTAINER    Gregory Mierzwinski <gmierzwinski@mozilla.com>

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

RUN apt-get -y update && \
    apt-get install -y python3.7 \
      python3.7-venv \
      python3.7-dev

CMD ["/bin/bash", "--login"]

