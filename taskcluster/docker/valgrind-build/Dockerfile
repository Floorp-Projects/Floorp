# TODO remove VOLUME below when the base image is updated next.
FROM          taskcluster/centos6-build-upd:0.1.8.20170808150401
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# Add build scripts; these are the entry points from the taskcluster worker, and
# operate on environment variables

# %include taskcluster/docker/desktop-build/bin
ADD topsrcdir/taskcluster/docker/desktop-build/bin /builds/worker/bin

RUN chmod +x /builds/worker/bin/*

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /builds/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/common.sh
ADD topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include taskcluster/docker/recipes/install-make.sh
ADD topsrcdir/taskcluster/docker/recipes/install-make.sh /setup/install-make.sh

# %include taskcluster/docker/recipes/install-cmake.sh
ADD topsrcdir/taskcluster/docker/recipes/install-cmake.sh /setup/install-cmake.sh

# %include taskcluster/docker/recipes/centos6-build-system-setup.sh
ADD topsrcdir/taskcluster/docker/recipes/centos6-build-system-setup.sh /setup/system-setup.sh

# %include taskcluster/docker/recipes/centos-install-debug-symbols.sh
ADD topsrcdir/taskcluster/docker/recipes/centos-install-debug-symbols.sh /setup/install-debug-symbols.sh

# TODO remove once base image doesn't install Mercurial
RUN pip uninstall -y Mercurial

RUN bash /setup/system-setup.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
ADD topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

# Add configuration
# %include taskcluster/docker/desktop-build/dot-config
ADD topsrcdir/taskcluster/docker/desktop-build/dot-config /builds/worker/.config

# Generate machine uuid file
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
# %include taskcluster/docker/desktop-build/oauth.txt
ADD topsrcdir/taskcluster/docker/desktop-build/oauth.txt /builds/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
# %include taskcluster/docker/desktop-build/buildprops.json
ADD topsrcdir/taskcluster/docker/desktop-build/buildprops.json /builds/worker/

# Move installation to base centos6-build image once Bug 1272629 is fixed
# Install the screen package here to use with xvfb.
# Install bison to build binutils.
RUN yum install -y bison screen

# Install libtool.
RUN yum install -y libtool

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
