FROM          centos:centos6
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>

# Run system setup script; this ensures taht the whole process
# boils down to a single docker layer
ADD           system-setup.sh   /tmp/system-setup.sh
RUN           ["/tmp/system-setup.sh"]

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

# Declare default working folder
WORKDIR       /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
