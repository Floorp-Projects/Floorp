FROM          ubuntu:16.04
MAINTAINER    Andrew Halberstadt <ahalberstadt@mozilla.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

RUN mkdir /build
# %include testing/docker/decision/tooltool.py
ADD topsrcdir/testing/docker/decision/tooltool.py /build/tooltool.py

# %include testing/docker/recipes/install-mercurial.sh
ADD topsrcdir/testing/docker/recipes/install-mercurial.sh /build/install-mercurial.sh
ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
