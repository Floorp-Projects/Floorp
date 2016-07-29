FROM          ubuntu:16.04
MAINTAINER    Greg Arndt <garndt@mozilla.com>

# Add worker user
RUN useradd -d /home/worker -s /bin/bash -m worker
RUN mkdir /home/worker/artifacts && chown worker:worker /home/worker/artifacts

# %include testing/docker/recipes/tooltool.py
ADD topsrcdir/testing/docker/recipes/tooltool.py /tmp/tooltool.py

ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

ADD bin /home/worker/bin

ENV PATH /home/worker/bin:$PATH
ENV SHELL /bin/bash
ENV HOME /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
