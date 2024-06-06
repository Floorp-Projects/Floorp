FROM          $DOCKER_IMAGE_PARENT
MAINTAINER    Gabriele Svelto <gsvelto@mozilla.com>

RUN mkdir -p /builds
RUN id worker || useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# We need to declare all potentially cache volumes as caches. Also,
# making high I/O paths volumes increase I/O throughput because of
# AUFS slowness.
VOLUME /builds/worker/checkouts

RUN apt-get update && \
    apt-get install --no-install-recommends -y 7zip jq python3-pip wget && \
    apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*
RUN pip3 install --progress-bar off --break-system-packages crashstats-tools==2.0.0

# %include tools/crashreporter/system-symbols/win
COPY topsrcdir/tools/crashreporter/system-symbols/win /builds/worker

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
