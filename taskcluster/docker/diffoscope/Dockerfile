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
      default-jdk-headless \
      python3-progressbar \
      unzip \
      zip \
      && \
    sed -i '/en_US.UTF-8/s/^# *//' /etc/locale.gen && \
    locale-gen

COPY get_and_diffoscope /builds/worker/bin/get_and_diffoscope
COPY readelf /builds/worker/bin/readelf
COPY report_error /builds/worker/bin/report_error
COPY test_diffoscope /builds/worker/bin/test_diffoscope

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/* && \
    /builds/worker/bin/test_diffoscope
