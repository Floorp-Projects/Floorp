FROM          ubuntu:14.04
MAINTAINER    Greg Arndt <garndt@mozilla.com>

# install non-build specific dependencies in a single layer
ADD system-setup.sh   /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

ENV PATH /home/worker/bin:/usr/local/lib/python2.7.10/bin:$PATH
ENV SHELL /bin/bash
ENV HOME /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
