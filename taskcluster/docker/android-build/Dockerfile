# TODO remove VOLUME below when the base image is updated next.
FROM          taskcluster/centos6-build-upd:0.1.8.20170808150401
MAINTAINER    Nick Alexander <nalexander@mozilla.com>

# BEGIN ../desktop-build/Dockerfile

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# %include python/mozbuild/mozbuild/action/tooltool.py
COPY topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /build/tooltool.py
COPY topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /builds/tooltool.py
COPY topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /setup/tooltool.py
COPY topsrcdir/python/mozbuild/mozbuild/action/tooltool.py /tmp/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
COPY topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include taskcluster/docker/recipes/common.sh
COPY topsrcdir/taskcluster/docker/recipes/common.sh /setup/common.sh

# %include taskcluster/docker/recipes/install-mercurial.sh
COPY topsrcdir/taskcluster/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include taskcluster/docker/recipes/install-make.sh
COPY topsrcdir/taskcluster/docker/recipes/install-make.sh /setup/install-make.sh

# %include taskcluster/docker/recipes/install-cmake.sh
COPY topsrcdir/taskcluster/docker/recipes/install-cmake.sh /setup/install-cmake.sh

# %include taskcluster/docker/recipes/centos6-build-system-setup.sh
COPY topsrcdir/taskcluster/docker/recipes/centos6-build-system-setup.sh /setup/system-setup.sh

# %include taskcluster/docker/recipes/centos-install-debug-symbols.sh
COPY topsrcdir/taskcluster/docker/recipes/centos-install-debug-symbols.sh /setup/install-debug-symbols.sh

# TODO remove once base image doesn't install Mercurial
RUN pip uninstall -y Mercurial

RUN bash /setup/system-setup.sh

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/run-task
COPY topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

# Add configuration
COPY            dot-config                    /builds/worker/.config

# Generate machine uuid file
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

# Stubbed out credentials; mozharness looks for this file an issues a WARNING
# if it's not found, which causes the build to fail.  Note that this needs to
# be in the parent of the workspace directory and in the directory where
# mozharness is run (not its --work-dir).  See Bug 1169652.
ADD           oauth.txt /builds/worker/

# stubbed out buildprops, which keeps mozharness from choking
# Note that this needs to be in the parent of the workspace directory and in
# the directory where mozharness is run (not its --work-dir)
ADD           buildprops.json /builds/worker/

# END ../desktop-build/Dockerfile

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Update base.
RUN rpm --rebuilddb && yum upgrade -y

# Install Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

# Install the screen package here to use with xvfb.
# Move installation to base centos6-build image once Bug 1272629 is fixed
# tzdata-java is symlinked from the JRE: see https://bugzilla.redhat.com/show_bug.cgi?id=1130800
RUN rpm --rebuilddb && yum install -y \
  createrepo \
  curl \
  screen \
  sudo \
  tar \
  tzdata-java \
  unzip \
  wget \
  zip \
  && yum clean all

ENV NEXUS_VERSION 2.12.0-01
ENV NEXUS_SHA1SUM 1a9aaad8414baffe0a2fd46eed1f41b85f4049e6

RUN mkdir -p /opt/sonatype/nexus

WORKDIR /tmp
RUN curl --fail --silent --location --retry 3 \
    https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz \
    -o /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# Observe the two spaces below.  Seriously.
RUN echo "${NEXUS_SHA1SUM}  nexus-${NEXUS_VERSION}-bundle.tar.gz" > nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1
RUN sha1sum --check nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1

RUN tar zxf nexus-${NEXUS_VERSION}-bundle.tar.gz \
  && mv /tmp/nexus-${NEXUS_VERSION}/* /opt/sonatype/nexus/ \
  && rm -rf /tmp/nexus-${NEXUS_VERSION} \
  && rm -rf /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# So that we don't have to RUN_AS_USER=root.
RUN chown -R worker:worker /opt/sonatype/nexus/

# Back to the centos6-build workdir, matching desktop-build.
WORKDIR /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
