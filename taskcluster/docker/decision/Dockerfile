FROM          ubuntu:16.04
MAINTAINER    Greg Arndt <garndt@mozilla.com>

# Add worker user
RUN mkdir /builds
RUN useradd -d /builds/worker -s /bin/bash -m worker -u 1000 -g 1000
RUN mkdir /builds/worker/artifacts && chown worker:worker /builds/worker/artifacts

# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /tmp/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /tmp/install-mercurial.sh

ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

ENV PATH /builds/worker/bin:$PATH
ENV SHELL /bin/bash
ENV HOME /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
