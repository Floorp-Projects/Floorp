FROM          taskcluster/base-test:0.1.3
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
COPY           dot-config                    /home/worker/.config
COPY           dot-pulse                     /home/worker/.pulse
COPY           bin                           /home/worker/bin
ADD            https://s3-us-west-2.amazonaws.com/test-caching/packages/linux64-stackwalk /usr/local/bin/linux64-minidump_stackwalk
ADD            https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /home/worker/bin/buildbot_step
COPY           tc-vcs-config.yml /etc/taskcluster-vcs.yml

# Run test setup script
RUN chmod u+x /home/worker/bin/buildbot_step
RUN chmod u+x /usr/local/bin/linux64-minidump_stackwalk
RUN apt-get install -y python-pip && pip install virtualenv;
RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts
RUN npm install -g taskcluster-vcs@2.3.12
RUN npm install -g taskcluster-npm-cache@1.3.3
RUN npm install -g node-gyp
RUN rm -Rf .cache && mkdir -p .cache
ENV PATH $PATH:/home/worker/bin
ENV MINIDUMP_STACKWALK /usr/local/bin/linux64-minidump_stackwalk

# Remove once running under 'worker' user.  This is necessary for pulseaudio to start
# XXX: change this back to worker:worker once permissions issues are resolved
RUN            chown -R root:root /home/worker


# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
