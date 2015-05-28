FROM          quay.io/mozilla/ubuntu-build:0.0.1
MAINTAINER    Morgan Reece Phillips <winter2718@gmail.com>

# Add build scripts; these are the entry points from the taskcluster worker, and
# operate on environment variables
ADD             bin /home/worker/bin
RUN             chmod +x /home/worker/bin/*

# Add custom mozharness configs
ADD             configs /home/worker/configs

##
# COMPILER HACKS
##
ADD             hacks.sh /tmp/
RUN             /tmp/hacks.sh

# So that the compiler can find installed libs
ENV           LIBRARY_PATH  /usr/lib/x86_64-linux-gnu

# Stubbed out credentials, which will force the upload step to be skipped.
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD           oauth.txt /home/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD           buildprops.json /home/worker/

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
