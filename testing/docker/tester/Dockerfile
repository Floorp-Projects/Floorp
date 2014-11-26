FROM          quay.io/mozilla/gecko-base
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Add utilities and configuration
COPY           fetch-artifacts.py            /home/worker/fetch-artifacts.py
COPY           b2g-desktop-tests.sh          /home/worker/b2g-desktop-tests.sh
COPY           b2g-emulator-tests.sh         /home/worker/b2g-emulator-tests.sh
COPY           b2g-desktop-config.py         /home/worker/b2g-desktop-config.py
COPY           test-setup.sh                 /tmp/test-setup.sh
COPY           dot-config                    /home/worker/.config
COPY           dot-pulse                     /home/worker/.pulse
COPY           emulator_automation_config.py /home/worker/emulator_automation_config.py
COPY           bin                           /home/worker/bin

# Run test setup script
USER          root
RUN           ["/tmp/test-setup.sh"]
ENV           PATH $PATH:/home/worker/bin

USER          worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
