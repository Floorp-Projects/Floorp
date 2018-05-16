FROM          ubuntu:16.04
MAINTAINER    Johan Lorenzo <jlorenzo+tc@mozilla.com>

RUN mkdir /builds
RUN groupadd -g 1000 worker
RUN useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker

RUN apt-get update
RUN apt-get install --yes git python3-setuptools build-essential libssl-dev libffi-dev python3-dev

WORKDIR /builds/worker/
RUN git clone https://github.com/mozilla-releng/mozapkpublisher
WORKDIR /builds/worker/mozapkpublisher
# Change hash_for_given_tag to point to a newer tag. For more information, see
# https://bugzilla.mozilla.org/show_bug.cgi?id=1459980#c3
RUN git checkout 50750fceb7c0d831cf61cb550af62b2c93e8e755
RUN python3 setup.py develop

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
