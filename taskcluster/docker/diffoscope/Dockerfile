FROM debian:stretch-20171210
MAINTAINER Mike Hommey <mhommey@mozilla.com>

RUN mkdir /builds
RUN useradd -d /builds/worker -s /bin/bash -m worker
WORKDIR /builds/worker

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV HOME=/builds/worker \
    SHELL=/bin/bash \
    USER=worker \
    LOGNAME=worker \
    HOSTNAME=taskcluster-worker \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

# Set apt sources list to a snapshot.
RUN for s in debian_stretch debian_stretch-updates debian-security_stretch/updates; do \
      echo "deb [check-valid-until=no] http://snapshot.debian.org/archive/${s%_*}/20171222T153610Z/ ${s#*_} main"; \
    done > /etc/apt/sources.list

RUN apt-get update -q && \
    apt-get install -yyq diffoscope libc++abi1 locales python3-setuptools python2.7 python-pip git && \
    sed -i '/en_US.UTF-8/s/^# *//' /etc/locale.gen && \
    locale-gen && \
    git clone https://anonscm.debian.org/git/reproducible/diffoscope.git /tmp/diffoscope && \
    git -C /tmp/diffoscope checkout 202caf9d5d134e95f870d5f19f89511d635c27e4 && \
    (cd /tmp/diffoscope && python3 setup.py install ) && \
    rm -rf /tmp/diffoscope && \
    apt-get clean

# %include taskcluster/docker/recipes/run-task
COPY topsrcdir/taskcluster/docker/recipes/run-task /builds/worker/bin/run-task

COPY get_and_diffoscope /builds/worker/bin/get_and_diffoscope

RUN chown -R worker:worker /builds/worker/bin && chmod 755 /builds/worker/bin/*

