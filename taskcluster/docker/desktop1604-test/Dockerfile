FROM          ubuntu:16.04
MAINTAINER    Joel Maher <joel.maher@gmail.com>


RUN mkdir /builds
RUN useradd -d /builds/worker -s /bin/bash -m worker
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

# Add the tooltool manifest containing the minidump_stackwalk binary.
# %include testing/config/tooltool-manifests/linux64/releng.manifest
ADD topsrcdir/testing/config/tooltool-manifests/linux64/releng.manifest /tmp/minidump_stackwalk.manifest

# %include taskcluster/docker/recipes/ubuntu1604-test-system-setup.sh
ADD topsrcdir/taskcluster/docker/recipes/ubuntu1604-test-system-setup.sh /setup/system-setup.sh
RUN           bash /setup/system-setup.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
ADD topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

# %include taskcluster/scripts/tester/test-linux.sh
ADD topsrcdir/taskcluster/scripts/tester/test-linux.sh /builds/worker/bin/test-linux.sh

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
COPY           dot-files/config              /builds/worker/.config
COPY           dot-files/pulse               /builds/worker/.pulse
RUN            chmod +x bin/*
# TODO: remove this when buildbot is gone
COPY           buildprops.json               /builds/worker/buildprops.json

# TODO: remove
ADD            https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /builds/worker/bin/buildbot_step
RUN chmod u+x /builds/worker/bin/buildbot_step

# allow the worker user to access video devices
RUN usermod -a -G video worker

RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts

# install tc-npm-cache
RUN  npm install -g taskcluster-npm-cache@1.1.14 \
 && rm -rf ~/.npm
ENV PATH $PATH:/builds/worker/bin

# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# Disable Ubuntu update prompt
# http://askubuntu.com/questions/515161/ubuntu-12-04-disable-release-notification-of-14-04-in-update-manager
ADD release-upgrades /etc/update-manager/release-upgrades

# Disable tools with on-login popups that interfere with tests; see bug 1240084 and bug 984944.
ADD autostart/jockey-gtk.desktop autostart/deja-dup-monitor.desktop /etc/xdg/autostart/

# Bug 1345105 - Do not run periodical update checks and downloads
ADD autostart/gnome-software-service.desktop /etc/xdg/autostart/

# In test.sh we accept START_VNC to start a vnc daemon.
# Exposing this port allows it to work.
EXPOSE 5900

# This helps not forgetting setting DISPLAY=:0 when running
# tests outside of test.sh
ENV DISPLAY :0

# Disable apport (Ubuntu app crash reporter) to avoid stealing focus from test runs
ADD apport /etc/default/apport

# Disable font antialiasing for now to match releng's setup
ADD fonts.conf /builds/worker/.fonts.conf

# Set up first-run experience for interactive mode
ADD motd /etc/taskcluster-motd
ADD taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN chmod +x /bin/taskcluster-interactive-shell

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
