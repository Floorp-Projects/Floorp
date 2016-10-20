FROM ubuntu:16.04

# %include taskcluster/docker/recipes/tooltool.py
ADD topsrcdir/taskcluster/docker/recipes/tooltool.py /setup/tooltool.py

# %include taskcluster/docker/recipes/common.sh
ADD topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include taskcluster/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/taskcluster/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /usr/local/bin/run-task

# Add and run setup script
ADD build-image.sh      /usr/local/bin/build-image.sh
ADD setup.sh            /setup/setup.sh
RUN bash /setup/setup.sh

# Setup a workspace that won't use AUFS
VOLUME /home/worker/workspace

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LC_ALL        C

# Create worker user
RUN useradd -d /home/worker -s /bin/bash -m worker

# Set some sane defaults
WORKDIR /home/worker/
CMD     build-image.sh
