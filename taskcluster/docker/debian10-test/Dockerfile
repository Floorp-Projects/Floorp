# The base image is a last known working snapshot of this image. We do this
# to work around problems rebuilding desktop1604-test. See bug 1503756 for
# the ugly details. Bug 1511527 tracks undoing this hack and building from
# a fresh ubuntu image.
FROM          debian:buster
MAINTAINER    Edwin Takahashi <egao@mozilla.com>

RUN mkdir -p /builds
RUN id worker || useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# %include taskcluster/docker/recipes/common.sh
ADD topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /setup/install-node.sh

# %include taskcluster/docker/recipes/debian-test-system-setup.sh
ADD topsrcdir/taskcluster/docker/recipes/debian-test-system-setup.sh /setup/system-setup.sh
RUN           bash /setup/system-setup.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
ADD topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content

# %include taskcluster/scripts/tester/test-linux.sh
ADD topsrcdir/taskcluster/scripts/tester/test-linux.sh /builds/worker/bin/test-linux.sh

ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /builds/worker/scripts/tooltool.py

# Locale related setup for debian:buster
RUN         echo "LC_ALL=en_US.UTF-8" >> /etc/environment
RUN         echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen
RUN         echo "LANG=en_US.UTF-8" > /etc/locale.conf
RUN         locale-gen en_US.UTF-8

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /builds/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Add utilities and configuration
COPY         dot-files/config              /builds/worker/.config
COPY         dot-files/pulse               /builds/worker/.pulse
RUN          chmod +x bin/*

# allow the worker user to access video devices
RUN usermod -a -G video worker

RUN mkdir -p artifacts

ENV PATH $PATH:/builds/worker/bin

# In test.sh we accept START_VNC to start a vnc daemon.
# Exposing this port allows it to work.
EXPOSE 5900

# This helps not forgetting setting DISPLAY=:0 when running
# tests outside of test.sh
ENV DISPLAY :0

# Disable apport (app crash reporter) to avoid stealing focus from test runs
ADD apport /etc/default/apport

# Disable font antialiasing for now to match releng's setup
ADD fonts.conf /builds/worker/.fonts.conf

# Set up first-run experience for interactive mode
ADD motd /etc/taskcluster-motd
ADD taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN chmod +x /bin/taskcluster-interactive-shell

RUN chown -R worker:worker /builds/worker

# gnome-keyring-daemon is configured to have the IPC_LOCK capability (to lock pages with secrets in
# memory), but docker isn't run with that capability granted. So, if we were to try running
# gnome-keyring-daemon without first clearing the capability, it would just exit with the message
# "Operation not permitted". Luckily it doesn't actually require the capability.
RUN setcap -r /usr/bin/gnome-keyring-daemon

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
