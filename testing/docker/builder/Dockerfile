FROM          quay.io/mozilla/base-build:0.0.1
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

ENV PATH /tools/python27-mercurial/bin:/tools/python27/bin:/tools/tools/buildfarm/utils:$PATH:/home/worker/bin/
ENV PYTHONPATH /tools/tools/lib/python:$PYTHONPATH

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

