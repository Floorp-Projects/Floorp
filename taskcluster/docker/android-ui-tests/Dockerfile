ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

LABEL authors="Richard Pappalardo <rpappalax@gmail.com>, Aaron Train <atrain@mozilla.com>"
LABEL maintainer="Richard Pappalardo <rpappalax@gmail.com>"

#----------------------------------------------------------------------------------------------------------------------
#-- Test tools --------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN chown -R worker:worker ${HOME}/.config

USER worker:worker

ENV GOOGLE_SDK_DOWNLOAD ./gcloud.tar.gz
ENV GOOGLE_SDK_VERSION 478

ENV TEST_TOOLS /builds/worker/test-tools
ENV PATH ${PATH}:${TEST_TOOLS}:${TEST_TOOLS}/google-cloud-sdk/bin

RUN mkdir -p ${TEST_TOOLS} && \
    mkdir -p ${HOME}/.config/gcloud

RUN curl https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-${GOOGLE_SDK_VERSION}.0.0-linux-x86_64.tar.gz --output ${GOOGLE_SDK_DOWNLOAD} \
    && tar -xvf ${GOOGLE_SDK_DOWNLOAD} -C ${TEST_TOOLS} \
    && rm -f ${GOOGLE_SDK_DOWNLOAD} \
    && ${TEST_TOOLS}/google-cloud-sdk/install.sh --quiet \
    && ${TEST_TOOLS}/google-cloud-sdk/bin/gcloud --quiet components update

ENV FLANK_DOWNLOAD ${TEST_TOOLS}/flank.jar
ENV FLANK_VERSION 23.10.1
RUN curl -L https://github.com/Flank/flank/releases/download/v${FLANK_VERSION}/flank.jar --output ${FLANK_DOWNLOAD} \
    && chmod +x ${FLANK_DOWNLOAD}

# Used in parse-ui-test-fromfile.py
RUN pip3 install --break-system-packages junitparser beautifultable

# Used for env var ingest in testrail.py
RUN pip3 install --break-system-packages python-dotenv

# run-task expects to run as root
USER root
