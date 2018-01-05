FROM          centos:6
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

RUN mkdir /builds

### add worker user and setup its workspace
RUN useradd -d /builds/worker -s /bin/bash -m worker
# Declare default working folder
WORKDIR       /builds/worker

# This will create a host mounted filesystem when the cache is stripped
# on Try. This cancels out some of the performance losses of aufs. See
# bug 1291940.
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# %include build/valgrind/valgrind-epochs.patch
ADD topsrcdir/build/valgrind/valgrind-epochs.patch /tmp/valgrind-epochs.patch

# install non-build specific dependencies in a single layer
ADD           system-setup.sh   /tmp/system-setup.sh
RUN           bash /tmp/system-setup.sh

# Builds need the share module enabled
ADD           hgrc /builds/worker/.hgrc
RUN chown -R worker:worker /builds/worker/.hgrc

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /builds/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
