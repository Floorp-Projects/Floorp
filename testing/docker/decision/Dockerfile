FROM          quay.io/mozilla/base-build:0.0.1
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

ENV PATH /home/worker/bin/:$PATH

# Add utilities and configuration
RUN           npm install -g taskcluster-vcs@2.3.4
ADD           bin                   /home/worker/bin
