FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Andrew Halberstadt <ahalberstadt@mozilla.com>

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts

# We do want to install recommended packages.
RUN sed -i /APT::Install-Recommends/d /etc/apt/apt.conf.d/99taskcluster

RUN mkdir /build
# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /build/tooltool.py

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /build/install-node.sh

ADD system-setup.sh /tmp/system-setup.sh
# %include tools/lint/eslint/manifest.tt
ADD topsrcdir/tools/lint/eslint/manifest.tt /tmp/eslint.tt
# %include tools/lint/eslint/eslint-plugin-mozilla/manifest.tt
ADD topsrcdir/tools/lint/eslint/eslint-plugin-mozilla/manifest.tt /tmp/eslint-plugin-mozilla.tt
# %include tools/lint/spell/codespell_requirements.txt
ADD topsrcdir/tools/lint/spell/codespell_requirements.txt /tmp/codespell_requirements.txt
# %include tools/lint/tox/tox_requirements.txt
ADD topsrcdir/tools/lint/tox/tox_requirements.txt /tmp/tox_requirements.txt
RUN bash /tmp/system-setup.sh

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
