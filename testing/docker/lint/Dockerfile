FROM          node:4.2
MAINTAINER    Dave Townsend <dtownsend@oxymoronical.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# install necessary npm packages
RUN           npm install -g taskcluster-vcs@2.3.12
RUN           npm install -g eslint@2.9.0
RUN           npm install -g eslint-plugin-html@1.4.0
RUN           npm install -g eslint-plugin-react@4.2.3

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
