FROM          taskcluster/centos6-build-upd:20150930093701
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

# Add build scripts; these are the entry points from the taskcluster worker, and
# operate on environment variables
ADD             bin /home/worker/bin
RUN             chmod +x /home/worker/bin/*

# Generate machine uuid file
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
ADD           oauth.txt /home/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD           buildprops.json /home/worker/

# install tooltool directly from github where tooltool_wrapper.sh et al. expect
# to find it
RUN wget -O /builds/tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
RUN chmod +x /builds/tooltool.py

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
