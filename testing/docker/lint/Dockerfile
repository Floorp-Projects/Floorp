FROM          node:4.2
MAINTAINER    Dave Townsend <dtownsend@oxymoronical.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# install necessary npm packages
RUN           npm install -g taskcluster-vcs@2.3.12

# Install tooltool directly from github.
RUN mkdir /build
ADD https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py /build/tooltool.py
RUN chmod +rx /build/tooltool.py

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
