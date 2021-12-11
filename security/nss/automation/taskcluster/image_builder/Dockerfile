FROM ubuntu:16.04
MAINTAINER Tim Taubert <ttaubert@mozilla.com>

WORKDIR /home/worker

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y apt-transport-https apt-utils
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 36A1D7869245C8950F966E92D8576A8BA88D21E9 && \
    sh -c "echo deb https://get.docker.io/ubuntu docker main \
    > /etc/apt/sources.list.d/docker.list"
RUN apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 41BD8711B1F0EC2B0D85B91CF59CE3A8323293EE && \
    sh -c "echo deb http://ppa.launchpad.net/mercurial-ppa/releases/ubuntu xenial main \
    > /etc/apt/sources.list.d/mercurial.list"
RUN apt-get update && apt-get install -y \
    lxc-docker-1.6.1 \
    mercurial

ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
