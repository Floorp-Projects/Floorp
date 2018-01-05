FROM ubuntu:16.04

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py

# %include taskcluster/docker/recipes/common.sh
ADD topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /usr/local/bin/run-task

# Add and run setup script
ADD build-image.sh      /usr/local/bin/build-image.sh
ADD download-and-compress /usr/local/bin/download-and-compress
ADD setup.sh            /setup/setup.sh
RUN bash /setup/setup.sh

# Setup a workspace that won't use AUFS.
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /builds/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LC_ALL        C

# Create worker user
RUN useradd -d /builds/worker -s /bin/bash -m worker

# Set some sane defaults
WORKDIR /builds/worker/
CMD     build-image.sh
