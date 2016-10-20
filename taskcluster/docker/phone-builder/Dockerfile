FROM          taskcluster/builder:0.5.13
MAINTAINER    Wander Lairson Costa <wcosta@mozilla.com>

# Add utilities and configuration
ADD           bin                   /home/worker/bin

# Builds need the share module enabled
ADD           hgrc /home/worker/.hgrc
RUN chown -R worker:worker /home/worker/.hgrc

# Make sure we use our own config
COPY          tc-vcs-config.yml /etc/taskcluster-vcs.yml

RUN           yum install -y bc lzop java-1.7.0-openjdk
RUN           pip install awscli
RUN           npm install -g bower gulp apm grunt-cli

