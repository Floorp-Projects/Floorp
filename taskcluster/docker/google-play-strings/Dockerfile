FROM          ubuntu:16.04
MAINTAINER    Johan Lorenzo <jlorenzo+tc@mozilla.com>

RUN mkdir /builds
RUN groupadd -g 1000 worker
RUN useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker

RUN apt-get update && apt-get install -y \
    build-essential \
    curl \
    libffi-dev \
    libfreetype6-dev \
    libpng12-dev \
    libssl-dev \
    libxml2-dev \
    libxslt1-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    python3-setuptools \
    unzip

WORKDIR /builds/worker/
# Change hash_for_given_tag to point to a newer tag. For more information, see
# https://bugzilla.mozilla.org/show_bug.cgi?id=1459980#c3
RUN curl --location https://github.com/mozilla-releng/mozapkpublisher/archive/8051c7f98959e2f041ce63a1911ade88827c3a66.zip > mozapkpublisher.zip
RUN unzip mozapkpublisher.zip
RUN mv mozapkpublisher-8051c7f98959e2f041ce63a1911ade88827c3a66 mozapkpublisher

WORKDIR /builds/worker/mozapkpublisher
RUN pip3 install --upgrade pip
RUN pip3 install --no-deps --require-hashes --requirement requirements.txt
RUN pip3 install --no-deps --editable .

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
