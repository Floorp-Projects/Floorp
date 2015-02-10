FROM          quay.io/mozilla/base-test:0.0.1
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
COPY           b2g-desktop-config.py         /home/worker/b2g-desktop-config.py
COPY           dot-config                    /home/worker/.config
COPY           dot-pulse                     /home/worker/.pulse
COPY           bin                           /home/worker/bin
COPY           mozharness_configs            /home/worker/mozharness_configs
COPY           buildprops.json               /home/worker/buildprops.json
ADD            https://raw.githubusercontent.com/taskcluster/buildbot-step/master/buildbot_step /home/worker/bin/buildbot_step


# Run test setup script
RUN chmod u+x /home/worker/bin/buildbot_step
RUN pip install virtualenv;
RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts
RUN chown -R worker:worker /home/worker/* /home/worker/.*
RUN npm install -g taskcluster-vcs@2.2.0
ENV PATH $PATH:/home/worker/bin

# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
