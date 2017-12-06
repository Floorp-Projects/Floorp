FROM ubuntu:vivid

# Ubuntu Vivid has been moved to the old-releases repo
RUN sed -i -e 's/archive.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list
RUN apt-get -q update \
    && apt-get install --yes -q \
    mercurial \
    python-dev \
    python-pip \
    python-virtualenv \
    libffi-dev \
    liblzma-dev \
    libssl-dev \
    libyaml-dev \
    libmysqlclient-dev \
    clamav \
    clamav-freshclam \
    curl \
    wget \
    && apt-get clean

COPY requirements.txt /tmp/
RUN pip install -r /tmp/requirements.txt
# Freshclam may be flaky, retry if it fails
RUN for i in 1 2 3 4 5; do freshclam --verbose && break || sleep 15; done
