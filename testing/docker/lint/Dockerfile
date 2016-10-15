FROM          ubuntu:16.04
MAINTAINER    Andrew Halberstadt <ahalberstadt@mozilla.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

RUN mkdir /build
# %include testing/docker/recipes/tooltool.py
ADD topsrcdir/testing/docker/recipes/tooltool.py /build/tooltool.py

# %include testing/mozharness/external_tools/robustcheckout.py
ADD topsrcdir/testing/mozharness/external_tools/robustcheckout.py /usr/local/mercurial/robustcheckout.py

# %include testing/docker/recipes/install-mercurial.sh
ADD topsrcdir/testing/docker/recipes/install-mercurial.sh /build/install-mercurial.sh
ADD system-setup.sh /tmp/system-setup.sh
# %include tools/lint/flake8/flake8_requirements.txt
ADD topsrcdir/tools/lint/flake8/flake8_requirements.txt /tmp/flake8_requirements.txt
RUN bash /tmp/system-setup.sh

# %include testing/docker/recipes/run-task
ADD topsrcdir/testing/docker/recipes/run-task /home/worker/bin/run-task
RUN chown -R worker:worker /home/worker/bin && chmod 755 /home/worker/bin/*

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
