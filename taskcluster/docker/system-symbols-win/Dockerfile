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
    apt-get install --no-install-recommends -y gcc python3-dev python3-pip python3-setuptools libffi-dev && \
    rm -rf /var/lib/apt/lists/*
RUN pip3 install --break-system-packages --no-cache-dir --require-hashes -r /builds/worker/requirements.txt

# %include tools/crashreporter/system-symbols/win
COPY topsrcdir/tools/crashreporter/system-symbols/win /builds/worker

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
