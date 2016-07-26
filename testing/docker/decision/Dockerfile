FROM          ubuntu:16.04
MAINTAINER    Greg Arndt <garndt@mozilla.com>

# Add worker user
RUN useradd -d /home/worker -s /bin/bash -m worker
RUN mkdir /home/worker/artifacts && chown worker:worker /home/worker/artifacts

ADD system-setup.sh tooltool.py /tmp/
RUN bash /tmp/system-setup.sh

ADD bin /home/worker/bin

ENV PATH /home/worker/bin:$PATH
ENV SHELL /bin/bash
ENV HOME /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
