FROM $DOCKER_IMAGE_PARENT
MAINTAINER Tom Ritter <tom@mozilla.com>

VOLUME /builds/worker/checkouts

# %include taskcluster/docker/recipes/install-node.sh
ADD topsrcdir/taskcluster/docker/recipes/install-node.sh /setup/install-node.sh

COPY    privileged-setup.sh     /setup/privileged-setup.sh
COPY    updatebot-version.sh    /setup/updatebot-version.sh
COPY    setup.sh                /builds/worker/setup.sh
COPY    run.py                  /builds/worker/run.py
COPY    hgrc                    /etc/mercurial/hgrc.d/updatebot.rc

RUN     cd /setup && ./privileged-setup.sh

ENV     HOME                /builds/worker
ENV     SHELL               /bin/bash
ENV     USER                worker
ENV     LOGNAME             worker
ENV     PYTHONUNBUFFERED    1
ENV     PATH                "/builds/worker/go/bin:/builds/worker/fetches/rustc/bin:/builds/worker/fetches/cargo-vet:${PATH}"

RUN     cd /builds/worker && ./setup.sh
