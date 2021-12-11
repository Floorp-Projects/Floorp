FROM $DOCKER_IMAGE_PARENT
MAINTAINER Simon Fraser <sfraser@mozilla.com>

ADD setup.sh /setup/setup.sh

RUN cd /setup && ./setup.sh

COPY runme.sh /
COPY scripts/* /home/worker/scripts/

CMD ["/runme.sh"]
