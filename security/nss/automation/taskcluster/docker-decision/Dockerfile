# Minimal image for running the decision task.
FROM ubuntu:18.04
LABEL maintainer="Martin Thomson <martin.thomson@gmail.com>"

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    locales \
    mercurial \
    nodejs \
 && rm -rf /var/lib/apt/lists/* \
 && apt-get autoremove -y && apt-get clean -y

ENV SHELL /bin/bash
ENV USER worker
ENV LOGNAME $USER
ENV HOME /home/$USER
ENV LANG en_US.UTF-8
ENV LC_ALL $LANG
ENV HOST localhost
ENV DOMSUF localdomain

RUN locale-gen $LANG \
 && DEBIAN_FRONTEND=noninteractive dpkg-reconfigure locales

RUN useradd -d $HOME -s $SHELL -m $USER
WORKDIR $HOME

# Add build and test scripts.
ADD bin $HOME/bin
RUN chmod +x $HOME/bin/*

USER $USER

# Set a default command for debugging.
CMD ["/bin/bash", "--login"]
