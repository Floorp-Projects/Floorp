FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Wes Kocher <wkocher@mozilla.com>

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

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
