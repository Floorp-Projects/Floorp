FROM ubuntu:14.04

WORKDIR /home/worker/bin

RUN apt-get update && apt-get install -y apt-transport-https
RUN sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 36A1D7869245C8950F966E92D8576A8BA88D21E9 && \
    sudo sh -c "echo deb https://get.docker.io/ubuntu docker main\
    > /etc/apt/sources.list.d/docker.list"
RUN apt-get update && apt-get install -y \
    lxc-docker-1.6.1 \
    curl \
    wget \
    git \
    mercurial \
    tar \
    zip \
    unzip \
    vim \
    sudo \
    ca-certificates \
    build-essential

ENV NODE_VERSION v0.12.4
RUN cd /usr/local/ && \
    curl https://nodejs.org/dist/$NODE_VERSION/node-$NODE_VERSION-linux-x64.tar.gz | tar -xz --strip-components 1 && \
    node -v

RUN npm install -g taskcluster-vcs@2.3.11

ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
