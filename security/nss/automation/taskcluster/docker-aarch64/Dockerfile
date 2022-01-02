FROM franziskus/xenial:aarch64
MAINTAINER Franziskus Kiefer <franziskuskiefer@gmail.com>

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker

# Add build and test scripts.
ADD bin /home/worker/bin
RUN chmod +x /home/worker/bin/*

# Install dependencies.
ADD setup.sh /tmp/setup.sh
RUN bash /tmp/setup.sh

# Change user.
# USER worker # See bug 1347473.

# Env variables.
ENV HOME /home/worker
ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME worker
ENV LANG en_US.UTF-8
ENV LC_ALL en_US.UTF-8
ENV HOST localhost
ENV DOMSUF localdomain

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
