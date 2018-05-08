FROM          ubuntu:16.04
MAINTAINER    Dustin Mitchell <dustin@mozilla.com>

# Add worker user
RUN mkdir /builds && \
    useradd -d /builds/worker -s /bin/bash -m worker && \
    mkdir /builds/worker/artifacts && \
    chown worker:worker /builds/worker/artifacts

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /tmp/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /tmp/install-mercurial.sh

ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task

ENV PATH=/builds/worker/bin:$PATH \
    SHELL=/bin/bash \
    HOME=/builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
