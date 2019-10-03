# %ARG DOCKER_IMAGE_PARENT
FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Wes Kocher <wkocher@mozilla.com>

RUN mkdir -p /builds
RUN id worker || useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

# %include taskcluster/docker/debian10-test-iris/install_iris_deps.sh
ADD topsrcdir/taskcluster/docker/debian10-test-iris/install_iris_deps.sh /setup/install_iris_deps.sh
RUN           bash /setup/install_iris_deps.sh

# Set up first-run experience for interactive mode
ADD motd /etc/taskcluster-motd
ADD taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN chmod +x /bin/taskcluster-interactive-shell

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
