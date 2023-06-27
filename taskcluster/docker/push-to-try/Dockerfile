# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

FROM $DOCKER_IMAGE_PARENT

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

RUN apt-get update && \
    apt-get install \
      openssh-client

COPY known_hosts /etc/ssh/ssh_known_hosts
COPY hgrc /etc/mercurial/hgrc.d/push-to-try.rc

RUN su worker -c "hg clone -r 19a77a5d7b2a90ba225cf5015afd1ac44787abec https://hg.mozilla.org/hgcustom/version-control-tools /builds/worker/.mozbuild/version-control-tools"

# Create directory for scripts to put an hg config in, for configuring ssh
RUN mkdir /builds/worker/.config/hg
RUN chown -R worker:worker /builds/worker/.config/hg
