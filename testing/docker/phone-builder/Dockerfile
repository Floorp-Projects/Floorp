FROM          taskcluster/builder:0.5.9
MAINTAINER    Wander Lairson Costa <wcosta@mozilla.com>

ENV           SOCORRO_SYMBOL_UPLOAD_TOKEN_FILE /home/worker/socorro.token

# Add utilities and configuration
ADD           bin                   /home/worker/bin
ADD           config                /home/worker/.aws/config
ADD           socorro.token         /home/worker/socorro.token

RUN           yum install -y bc lzop
RUN           pip install awscli
RUN           npm install -g bower gulp apm grunt-cli

# Set a default command useful for debugging
ENTRYPOINT ["validate_task.py"]

