# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER    Tarek Ziade <tarek@mozilla.com>

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

RUN add-apt-repository ppa:jonathonf/python-3.6
RUN apt-get -y update && \
    apt-get install -y python3.6 && \
    curl https://bootstrap.pypa.io/get-pip.py | python3.6

CMD ["/bin/bash", "--login"]

