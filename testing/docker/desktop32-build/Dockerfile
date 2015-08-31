FROM          taskcluster/ubuntu32-build:0.0.5
MAINTAINER    Morgan Reece Phillips <winter2718@gmail.com>

# Add build scripts; these are the entry points from the taskcluster worker, and
# operate on environment variables
ADD                 bin /home/worker/bin
RUN                 chmod +x /home/worker/bin/*

##
# COMPILER HACKS
##
ADD                 hacks.sh /tmp/
RUN                 /tmp/hacks.sh

# So that the compilers can find necessary libraries
# gcc
ENV LIBRARY_PATH    /usr/lib/i386-linux-gnu
ENV PKG_CONFIG_PATH /usr/lib/i386-linux-gnu/pkgconfig

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
ADD                 oauth.txt /home/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD                 buildprops.json /home/worker/

# Make sure commands from here on run as `worker`
# XXX but non-root tasks aren't yet supported - bug 1093833
# USER worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
