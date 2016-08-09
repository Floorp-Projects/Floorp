FROM          taskcluster/ubuntu1204-test-upd:0.1.11.20160628204600
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# This will create a host mounted filesystem when the cache is stripped
# on Try. This cancels out some of the performance losses of aufs. See
# bug 1291940.
VOLUME /home/worker/workspace

# Add utilities and configuration
COPY           dot-files/config              /home/worker/.config
COPY           dot-files/pulse               /home/worker/.pulse
COPY           dot-files/hgrc                /home/worker/.hgrc
COPY           bin                           /home/worker/bin
RUN            chmod +x bin/*
# TODO: remove this when buildbot is gone
COPY           buildprops.json               /home/worker/buildprops.json
COPY           tc-vcs-config.yml /etc/taskcluster-vcs.yml

# TODO: remove
ADD            https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /home/worker/bin/buildbot_step
RUN chmod u+x /home/worker/bin/buildbot_step

# TODO: remove
ADD            https://s3-us-west-2.amazonaws.com/test-caching/packages/linux64-stackwalk /usr/local/bin/linux64-minidump_stackwalk
RUN chmod +x /usr/local/bin/linux64-minidump_stackwalk

# allow the worker user to access video devices
RUN usermod -a -G video worker

RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts

# install tc-vcs and tc-npm-cache
RUN npm install -g taskcluster-vcs@2.3.12 \
 && npm install -g taskcluster-npm-cache@1.1.14 \
 && rm -rf ~/.npm
ENV PATH $PATH:/home/worker/bin

# Remove once running under 'worker' user.  This is necessary for pulseaudio to start
# XXX: change this back to worker:worker once permissions issues are resolved
RUN            chown -R root:root /home/worker


# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# clean up
RUN rm -Rf .cache && mkdir -p .cache

# Disable Ubuntu update prompt
# http://askubuntu.com/questions/515161/ubuntu-12-04-disable-release-notification-of-14-04-in-update-manager
ADD release-upgrades /etc/update-manager/release-upgrades

# Disable tools with on-login popups that interfere with tests; see bug 1240084 and bug 984944.
ADD jockey-gtk.desktop deja-dup-monitor.desktop /etc/xdg/autostart/

# In test.sh we accept START_VNC to start a vnc daemon.
# Exposing this port allows it to work.
EXPOSE 5900

# This helps not forgetting setting DISPLAY=:0 when running
# tests outside of test.sh
ENV DISPLAY :0

# Disable apport (Ubuntu app crash reporter) to avoid stealing focus from test runs
ADD apport /etc/default/apport

# Disable font antialiasing for now to match releng's setup
ADD fonts.conf /home/worker/.fonts.conf

# Set up first-run experience for interactive mode
ADD motd /etc/taskcluster-motd
ADD taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN chmod +x /bin/taskcluster-interactive-shell

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
