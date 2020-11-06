FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Gabriele Svelto <gsvelto@mozilla.com>

RUN mkdir -p /builds
RUN id worker || useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME /builds/worker/checkouts

COPY requirements.txt /builds/worker/requirements.txt
RUN apt-get update && \
    apt-get install --no-install-recommends -y gcc python3-dev python3-pip && \
    rm -rf /var/lib/apt/lists/*
RUN pip3 install --no-cache-dir --require-hashes -r /builds/worker/requirements.txt

# %include tools/crashreporter/system-symbols/win
COPY topsrcdir/tools/crashreporter/system-symbols/win /builds/worker

# Set up first-run experience for interactive mode
# %include taskcluster/docker/debian10-test/motd
COPY topsrcdir/taskcluster/docker/debian10-test/motd /etc/taskcluster-motd
# %include taskcluster/docker/debian10-test/taskcluster-interactive-shell
COPY topsrcdir/taskcluster/docker/debian10-test/taskcluster-interactive-shell /bin/taskcluster-interactive-shell
RUN chmod +x /bin/taskcluster-interactive-shell

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
