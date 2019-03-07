# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV LANG=en_US.UTF-8

RUN apt-get install \
      binutils-multiarch \
      bzip2 \
      curl \
      enjarify \
      diffoscope \
      jsbeautifier \
      libc++abi1 \
      locales \
      openjdk-8-jdk-headless \
      python3-progressbar \
      python3-setuptools \
      python2.7 \
      python-pip \
      unzip \
      zip \
      git \
      && \
    sed -i '/en_US.UTF-8/s/^# *//' /etc/locale.gen && \
    locale-gen && \
    git clone https://salsa.debian.org/reproducible-builds/diffoscope.git /tmp/diffoscope && \
    git -C /tmp/diffoscope checkout 202caf9d5d134e95f870d5f19f89511d635c27e4 && \
    (cd /tmp/diffoscope && python3 setup.py install ) && \
    rm -rf /tmp/diffoscope

COPY get_and_diffoscope /builds/worker/bin/get_and_diffoscope

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
