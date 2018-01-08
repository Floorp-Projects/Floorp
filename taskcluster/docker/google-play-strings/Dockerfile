FROM          ubuntu:16.04
MAINTAINER    Johan Lorenzo <jlorenzo+tc@mozilla.com>

RUN mkdir /builds
RUN groupadd -g 500 worker
RUN useradd -u 500 -g 500 -d /builds/worker -s /bin/bash -m worker

RUN apt-get update
RUN apt-get install --yes git python3-setuptools build-essential libssl-dev libffi-dev python3-dev

WORKDIR /builds/worker/
RUN git clone https://github.com/mozilla-releng/mozapkpublisher
WORKDIR /builds/worker/mozapkpublisher
RUN python3 setup.py develop

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
