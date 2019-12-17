FROM          ubuntu:16.04
MAINTAINER    Andrew Halberstadt <ahalberstadt@mozilla.com>

RUN mkdir /builds
RUN useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts

RUN mkdir /build
# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /build/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /build/install-node.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /build/install-mercurial.sh
ADD system-setup.sh /tmp/system-setup.sh
# %include tools/lint/eslint/manifest.tt
ADD topsrcdir/tools/lint/eslint/manifest.tt /tmp/eslint.tt
# %include tools/lint/eslint/eslint-plugin-mozilla/manifest.tt
ADD topsrcdir/tools/lint/eslint/eslint-plugin-mozilla/manifest.tt /tmp/eslint-plugin-mozilla.tt
# %include tools/lint/python/flake8_requirements.txt
ADD topsrcdir/tools/lint/python/flake8_requirements.txt /tmp/flake8_requirements.txt
# %include tools/lint/spell/codespell_requirements.txt
ADD topsrcdir/tools/lint/spell/codespell_requirements.txt /tmp/codespell_requirements.txt
# %include tools/lint/tox/tox_requirements.txt
ADD topsrcdir/tools/lint/tox/tox_requirements.txt /tmp/tox_requirements.txt
RUN bash /tmp/system-setup.sh

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task
RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /builds/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
