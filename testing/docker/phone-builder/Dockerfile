FROM          quay.io/mozilla/builder:0.5.4
MAINTAINER    Wander Lairson Costa <wcosta@mozilla.com>

# Add utilities and configuration
ADD           bin                   /home/worker/bin
ADD           config                /home/worker/.aws/config

RUN           yum install -y bc lzop
RUN           pip install awscli
RUN           npm install -g bower gulp apm grunt-cli

# Set a default command useful for debugging
ENTRYPOINT ["validate_task.py"]

