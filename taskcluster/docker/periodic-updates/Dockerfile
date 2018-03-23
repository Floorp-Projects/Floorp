FROM ubuntu:bionic
MAINTAINER Simon Fraser <sfraser@mozilla.com>

# Required software
ENV DEBIAN_FRONTEND noninteractive

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

ADD setup.sh /setup/setup.sh

RUN cd /setup && ./setup.sh

COPY runme.sh /
COPY scripts/* /home/worker/scripts/

ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

CMD ["/runme.sh"]
