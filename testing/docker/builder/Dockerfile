FROM          quay.io/mozilla/base:0.0.2
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

ENV PATH $PATH:/home/worker/bin/

# Add utilities and configuration
ADD           mozconfigs/           /home/worker/mozconfigs/
RUN           mkdir -p              /home/worker/bin
ADD           build.sh              /home/worker/bin/build.sh
ADD           build-b2g-desktop.sh  /home/worker/bin/build-b2g-desktop.sh
ADD           get-objdir.py         /home/worker/bin/get-objdir.py
ADD           get_gaia_repo.js      /home/worker/bin/get_gaia_repo.js
ADD           get_gaia_revision.js  /home/worker/bin/get_gaia_revision.js
ADD           build-setup.sh        /home/worker/bin/build-setup.sh

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
