FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Edwin Takahashi <egao@mozilla.com>

# Create necessary directories
RUN     mkdir -p /artifacts

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

# %include taskcluster/docker/recipes/common.sh
ADD topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /setup/install-node.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
ADD topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /builds/worker/scripts/tooltool.py

# %include build/psutil_requirements.txt
ADD topsrcdir/build/psutil_requirements.txt /setup/psutil_requirements.txt
# %include taskcluster/docker/recipes/ubuntu1804-test-system-setup.sh
ADD topsrcdir/taskcluster/docker/recipes/ubuntu1804-test-system-setup.sh /setup/system-setup.sh
RUN     bash /setup/system-setup.sh

# %include taskcluster/scripts/tester/test-linux.sh
ADD topsrcdir/taskcluster/scripts/tester/test-linux.sh /builds/worker/bin/test-linux.sh

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV     LANG            en_US.UTF-8
ENV     LC_ALL          en_US.UTF-8
ENV     PATH            $PATH:/builds/worker/bin

# This helps not forgetting setting DISPLAY=:0 when running
# tests outside of test.sh
ENV DISPLAY :0

# Add utilities and configuration
COPY    dot-files/config        /builds/worker/.config

# Disable apport (app crash reporter) to avoid stealing focus from test runs
ADD apport /etc/default/apport

# Disable font antialiasing for now to match releng's setup
ADD fonts.conf /builds/worker/.fonts.conf

# Bug 1345105 - Do not run periodical update checks and downloads
ADD autostart/gnome-software-service.desktop /etc/xdg/autostart/

# allow the worker user to access video devices
RUN     usermod -a -G video worker

# Set execution and ownership privileges
RUN     chmod +x bin/*; chown -R worker:worker /builds/worker

# Set up first-run experience for interactive mode
ADD motd /etc/taskcluster-motd
ADD taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN     chmod +x /bin/taskcluster-interactive-shell

# Bug 1638183 - increase xserver maximum client count
COPY     dot-files/config/xorg/99-serverflags.conf     /usr/share/X11/xorg.conf.d/

# Ubuntu 18.04 (or any other GNOME3 based systems) needs dbus to have various
# test function as expected. Use entrypoint to initialize dbus as root.
COPY dbus.sh /usr/local/bin/dbus.sh
RUN chmod +x /usr/local/bin/dbus.sh
ENTRYPOINT ["/usr/local/bin/dbus.sh"]

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
