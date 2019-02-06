# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM python:3.7-slim
MAINTAINER Mozilla Release Engineering "release+dockerhub@mozilla.com"

ENV PROJECT_REPOSITORY="https://github.com/mozilla-mobile/android-components.git" \
  TERM=dumb

RUN apt-get update -qq \
  && apt-get install -y git \
  && apt-get clean

WORKDIR /build/
COPY requirements.txt requirements.txt
RUN pip --no-cache-dir install --require-hashes --requirement requirements.txt

RUN git clone --depth=1 $PROJECT_REPOSITORY
WORKDIR /build/android-components
