FROM          ubuntu:16.04
MAINTAINER    Joel Maher <joel.maher@gmail.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# %include testing/docker/recipes/tooltool.py
ADD topsrcdir/testing/docker/recipes/tooltool.py /setup/tooltool.py

# %include testing/docker/recipes/install-mercurial.sh
ADD topsrcdir/testing/docker/recipes/install-mercurial.sh /setup/install-mercurial.sh

# %include testing/docker/recipes/ubuntu1604-test-system-setup.sh
ADD topsrcdir/testing/docker/recipes/ubuntu1604-test-system-setup.sh /setup/system-setup.sh
RUN           bash /setup/system-setup.sh

# %include testing/docker/recipes/run-task
ADD topsrcdir/testing/docker/recipes/run-task /home/worker/bin/run-task

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker
ENV           HOSTNAME      taskcluster-worker
ENV           LANG          en_US.UTF-8
ENV           LC_ALL        en_US.UTF-8

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
