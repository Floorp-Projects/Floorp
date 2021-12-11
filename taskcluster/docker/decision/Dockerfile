FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Andrew Halberstadt <ahal@mozilla.com>

RUN mkdir /builds/worker/artifacts && \
    chown worker:worker /builds/worker/artifacts

ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

ADD comm-task-env /builds/worker/bin/comm-task-env

ENV PATH=/builds/worker/bin:$PATH

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
