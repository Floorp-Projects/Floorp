FROM          quay.io/mozilla/base-test:0.0.1
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
ADD           b2g-desktop-config.py         /home/worker/b2g-desktop-config.py
ADD           dot-config                    /home/worker/.config
ADD           dot-pulse                     /home/worker/.pulse
ADD           bin                           /home/worker/bin
ADD           mozharness_configs            /home/worker/mozharness_configs

# Run test setup script
RUN pip install virtualenv;
RUN mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos; mkdir artifacts
RUN chown -R worker:worker /home/worker/* /home/worker/.*
RUN npm install -g taskcluster-vcs@2.1.3
ENV PATH $PATH:/home/worker/bin

# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
