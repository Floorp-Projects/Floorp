FROM          quay.io/mozilla/builder:0.2.6
MAINTAINER    Wander Lairson Costa <wcosta@mozilla.com>

# Add utilities and configuration
ADD           bin                   /home/worker/bin
ADD           config                /home/worker/.aws/config
ADD           system-setup.sh       /tmp/system-setup.sh

RUN           /tmp/system-setup.sh

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

