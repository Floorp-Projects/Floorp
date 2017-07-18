FROM          ubuntu:16.04
MAINTAINER    Andrew Halberstadt <ahalberstadt@mozilla.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

RUN mkdir /build
# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /build/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /build/install-node.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /build/install-mercurial.sh
ADD system-setup.sh /tmp/system-setup.sh
# %include tools/lint/eslint/manifest.tt
ADD topsrcdir/tools/lint/eslint/manifest.tt /tmp/eslint.tt
# %include tools/lint/eslint/eslint-plugin-mozilla/manifest.tt
ADD topsrcdir/tools/lint/eslint/eslint-plugin-mozilla/manifest.tt /tmp/eslint-plugin-mozilla.tt
# %include tools/lint/flake8_/flake8_requirements.txt
ADD topsrcdir/tools/lint/flake8_/flake8_requirements.txt /tmp/flake8_requirements.txt
# %include tools/lint/tox/tox_requirements.txt
ADD topsrcdir/tools/lint/tox/tox_requirements.txt /tmp/tox_requirements.txt
RUN bash /tmp/system-setup.sh

# %include taskcluster/docker/recipes/run-task
ADD topsrcdir/taskcluster/docker/recipes/run-task /home/worker/bin/run-task
RUN chown -R worker:worker /home/worker/bin && chmod 755 /home/worker/bin/*

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
