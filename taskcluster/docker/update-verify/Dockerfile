# This is an LTS! We should upgrade after the next LTS is released, unless
# we've switched to the in-tree debian base images by then.
FROM $DOCKER_IMAGE_PARENT

MAINTAINER release@mozilla.com

# We do want to install recommended packages.
RUN sed -i /APT::Install-Recommends/d /etc/apt/apt.conf.d/99taskcluster

RUN dpkg --add-architecture i386 && apt-get -q update \
    # p7zip-full is for extracting Windows and OS X packages
    # wget is for downloading update.xml, installers, and MARs
    # libgtk-3-0 is required to run the Firefox updater
    && apt-get -q --yes install p7zip-full wget libgtk-3-0 libgtk-3.0:i386 python3-cairo

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts

RUN mkdir /build
ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
ENV PATH $PATH:/builds/worker/bin

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
