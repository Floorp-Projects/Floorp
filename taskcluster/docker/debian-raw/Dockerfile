# %ARG BASE_IMAGE
FROM $BASE_IMAGE
MAINTAINER Mike Hommey <mhommey@mozilla.com>

ENV DEBIAN_FRONTEND=noninteractive

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]

COPY setup_packages.sh /usr/local/sbin/

# %ARG DIST
# %ARG SNAPSHOT
# Set apt sources list to a snapshot.
RUN for s in debian_$DIST debian_$DIST-updates debian_$DIST-backports debian-security_$DIST/updates; do \
      if [ $s = "debian_jessie-backports" ]; then \
          echo "deb http://snapshot.debian.org/archive/debian-archive/20190328T105444Z/debian/ ${s#*_} main"; \
      else \
          echo "deb http://snapshot.debian.org/archive/${s%_*}/$SNAPSHOT/ ${s#*_} main"; \
      fi; \
    done > /etc/apt/sources.list && \
    ( echo 'quiet "true";'; \
      echo 'APT::Get::Assume-Yes "true";'; \
      echo 'APT::Install-Recommends "false";'; \
      echo 'Acquire::Check-Valid-Until "false";'; \
      echo 'Acquire::Retries "5";'; \
    ) > /etc/apt/apt.conf.d/99taskcluster

RUN apt-get update && \
    apt-get dist-upgrade && \
    apt-get install \
      apt-transport-https \
      ca-certificates
