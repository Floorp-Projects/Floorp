FROM          ubuntu:18.04
MAINTAINER    Johan Lorenzo <jlorenzo+tc@mozilla.com>

RUN mkdir /builds
RUN groupadd -g 1000 worker
RUN useradd -u 1000 -g 1000 -d /builds/worker -s /bin/bash -m worker

RUN apt-get update \
  && apt-get install -y \
    build-essential \
    curl \
    libffi-dev \
    libfreetype6-dev \
    libpng-dev \
    libssl-dev \
    libxml2-dev \
    libxslt1-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    python3-setuptools \
    unzip \
  && apt-get clean

WORKDIR /builds/worker/
# Change hash_for_given_tag to point to a newer tag. For more information, see
# https://bugzilla.mozilla.org/show_bug.cgi?id=1459980#c3
RUN export revision='7647009c5a32e543bae941192071c66872733ed3' \
  && curl --location "https://github.com/mozilla-releng/mozapkpublisher/archive/$revision.zip" > mozapkpublisher.zip \
  && unzip mozapkpublisher.zip \
  && mv "mozapkpublisher-$revision" mozapkpublisher \
  && rm mozapkpublisher.zip

WORKDIR /builds/worker/mozapkpublisher
RUN pip3 --no-cache-dir install --upgrade pip
RUN pip3 --no-cache-dir install --no-deps --require-hashes --requirement requirements.txt
RUN pip3 --no-cache-dir install --no-deps --editable .

RUN chown -R worker:worker /builds/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
