FROM          quay.io/mozilla/base-test:0.0.1
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
ADD           b2g-desktop-config.py         /home/worker/b2g-desktop-config.py
ADD           test-setup.sh                 /tmp/test-setup.sh
ADD           dot-config                    /home/worker/.config
ADD           dot-pulse                     /home/worker/.pulse
ADD           bin                           /home/worker/bin
ADD           mozharness_configs            /home/worker/mozharness_configs

# Run test setup script
USER          root
RUN           ["/tmp/test-setup.sh"]
ENV           PATH $PATH:/home/worker/bin

# TODO Re-enable worker when bug 1093833 lands
#USER          worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
