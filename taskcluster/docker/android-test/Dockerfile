FROM          ubuntu:focal-20200606
MAINTAINER    Edwin Takahashi <egao@mozilla.com>

# Create necessary directories and worker user account
RUN     mkdir -p /builds && mkdir -p artifacts
RUN     id worker || useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME  /builds/worker/.cache
VOLUME  /builds/worker/checkouts
VOLUME  /builds/worker/tooltool-cache
VOLUME  /builds/worker/workspace

# In test.sh we accept START_VNC to start a vnc daemon.
# Exposing this port allows it to work.
EXPOSE  5900

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

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
ADD topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content

ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /builds/worker/scripts/tooltool.py

# %include taskcluster/docker/recipes/android-test-system-setup.sh
ADD topsrcdir/taskcluster/docker/recipes/android-test-system-setup.sh /setup/system-setup.sh
RUN     bash /setup/system-setup.sh

# %include taskcluster/scripts/tester/test-android.sh
ADD topsrcdir/taskcluster/scripts/tester/test-android.sh /builds/worker/bin/test-android.sh

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV     HOME            /builds/worker
ENV     SHELL           /bin/bash
ENV     USER            worker
ENV     LOGNAME         worker
ENV     HOSTNAME        taskcluster-worker
ENV     PATH            $PATH:/builds/worker/bin

# Bug 1345105 - Do not run periodical update checks and downloads
# %include taskcluster/docker/files/autostart/gnome-software-service.desktop
ADD topsrcdir/taskcluster/docker/files/autostart/gnome-software-service.desktop /etc/xdg/autostart/gnome-software-service.desktop

# Set execution and ownership privileges
RUN     chmod +x bin/*; chown -R worker:worker /builds/worker

# Set up first-run experience for interactive mode
# %include taskcluster/docker/files/motd
ADD topsrcdir/taskcluster/docker/files/motd /etc/taskcluster-motd
# %include taskcluster/docker/files/taskcluster-interactive-shell
ADD topsrcdir/taskcluster/docker/files/taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN     chmod +x /bin/taskcluster-interactive-shell

# Ubuntu 18.04 (or any other GNOME3 based systems) needs dbus to have various
# test function as expected. Use entrypoint to initialize dbus as root.
# %include taskcluster/docker/files/dbus.sh
ADD topsrcdir/taskcluster/docker/files/dbus.sh /usr/local/bin/dbus.sh
RUN     chmod +x /usr/local/bin/dbus.sh
ENTRYPOINT ["/usr/local/bin/dbus.sh"]

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
