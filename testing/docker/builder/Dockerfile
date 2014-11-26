FROM          quay.io/mozilla/base:0.0.3
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

ENV PATH $PATH:/home/worker/bin/
ENV TOOLTOOL_CACHE  /home/worker/tools/tooltool-cache

# Add utilities and configuration
ADD           system-setup.sh       /tmp/system-setup.sh
ADD           bin                   /home/worker/bin
RUN           /tmp/system-setup.sh

# TODO enable worker
# TODO volume mount permissions will be an issue
# USER worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

