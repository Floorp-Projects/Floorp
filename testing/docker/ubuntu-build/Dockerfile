FROM          ubuntu:14.04
MAINTAINER    Morgan Reece Phillips <winter2718@gmail.com>

### add worker user and setup its workspace
RUN useradd -d /home/worker -s /bin/bash -m worker

# install non-build specific dependencies in a single layer
ADD           system-setup.sh   /tmp/system-setup.sh
RUN           bash /tmp/system-setup.sh

# configure git and install tc-vcs
RUN git config --global user.email "nobody@mozilla.com" && \
    git config --global user.name "mozilla"
RUN npm install -g taskcluster-vcs@2.3.9

# Ensure that build specific dependencies live in a single layer
ADD           build-setup.sh   /tmp/build-setup.sh
RUN           bash /tmp/build-setup.sh

# Builds need the share module enabled
ADD           hgrc /home/worker/.hgrc

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker

# Declare default working folder
RUN chown -R worker:worker /home/worker/* /home/worker/.*
WORKDIR       /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
