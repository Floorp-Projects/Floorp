# This is an LTS! We should upgrade after the next LTS is released, unless
# we've switched to the in-tree debian base images by then.
FROM ubuntu:20.04

MAINTAINER release@mozilla.com

RUN dpkg --add-architecture i386 && apt-get -q update \
    # p7zip-full is for extracting Windows and OS X packages
    # wget is for downloading update.xml, installers, and MARs
    # libgtk-3-0 is required to run the Firefox updater
    && apt-get -q --yes install p7zip-full wget libgtk-3-0 libgtk-3.0:i386 \
    && apt-get clean

RUN mkdir /builds
RUN groupadd -g 1000 worker
RUN useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts

RUN mkdir /build
# %include python/mozbuild/mozbuild/action/tooltool.py
ADD topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /build/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/hgrc
COPY topsrcdir/taskcluster/docker/recipes/hgrc /etc/mercurial/hgrc.d/mozilla.rc

# %include taskcluster/docker/recipes/install-mercurial.sh
ADD topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /build/install-mercurial.sh
ADD system-setup.sh /tmp/system-setup.sh
RUN bash /tmp/system-setup.sh

# %include taskcluster/scripts/run-task
ADD topsrcdir/taskcluster/scripts/run-task /builds/worker/bin/run-task
# %include taskcluster/scripts/misc/fetch-content
ADD topsrcdir/taskcluster/scripts/misc/fetch-content /builds/worker/bin/fetch-content
RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*
ENV PATH $PATH:/builds/worker/bin

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /builds/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
