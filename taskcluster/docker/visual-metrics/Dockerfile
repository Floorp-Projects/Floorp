# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Barret Rennie <barret@mozilla.com>

RUN apt-get update && \
    apt-get install \
      ffmpeg \
      imagemagick \
      pyssim \
      python \
      python-pil \
      python3 \
      python3-pip

WORKDIR /builds/worker

# %include testing/mozharness/external_tools/performance-artifact-schema.json
ADD topsrcdir/testing/mozharness/external_tools/performance-artifact-schema.json /builds/worker/performance-artifact-schema.json

COPY requirements.txt /builds/worker/requirements.txt
RUN pip3 install setuptools==42.0.2
RUN pip3 install --require-hashes -r /builds/worker/requirements.txt && \
    rm /builds/worker/requirements.txt

COPY similarity.py /builds/worker/bin/similarity.py
COPY run-visual-metrics.py /builds/worker/bin/run-visual-metrics.py
RUN chmod +x /builds/worker/bin/run-visual-metrics.py

VOLUME /builds/worker/artifacts/
