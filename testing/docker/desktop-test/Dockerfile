FROM          taskcluster/ubuntu1204-test-upd:20151005212801
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
COPY           dot-config                    /home/worker/.config
COPY           dot-pulse                     /home/worker/.pulse
COPY           bin                           /home/worker/bin
# TODO: remove these (they should be in-tree)
COPY           mozharness_configs            /home/worker/mozharness_configs
# TODO: remove this when buildbot is gone
COPY           buildprops.json               /home/worker/buildprops.json
COPY           tc-vcs-config.yml /etc/taskcluster-vcs.yml

# TODO: remove
ADD            https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /home/worker/bin/buildbot_step
RUN chmod u+x /home/worker/bin/buildbot_step

# TODO: remove
ADD            https://s3-us-west-2.amazonaws.com/test-caching/packages/linux64-stackwalk /usr/local/bin/linux64-minidump_stackwalk
RUN chmod u+x /usr/local/bin/linux64-minidump_stackwalk

RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts

# install a new enough npm, plus tc-vcs and tc-npm-cache
RUN npm install -g npm@^2.0.0
RUN npm install -g taskcluster-vcs@2.3.12
RUN npm install -g taskcluster-npm-cache@1.1.14
ENV PATH $PATH:/home/worker/bin

# Remove once running under 'worker' user.  This is necessary for pulseaudio to start
# XXX: change this back to worker:worker once permissions issues are resolved
RUN            chown -R root:root /home/worker


# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# clean up
RUN rm -Rf .cache && mkdir -p .cache

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
