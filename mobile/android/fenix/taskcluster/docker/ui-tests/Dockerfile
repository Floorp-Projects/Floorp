FROM $DOCKER_IMAGE_PARENT

LABEL authors="Richard Pappalardo <rpappalax@gmail.com>, Aaron Train <atrain@mozilla.com>"
LABEL maintainer="Richard Pappalardo <rpappalax@gmail.com>"

#----------------------------------------------------------------------------------------------------------------------
#-- Test tools --------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

USER worker:worker

ENV GOOGLE_SDK_DOWNLOAD ./gcloud.tar.gz
ENV GOOGLE_SDK_VERSION 233
ENV FLANK_VERSION v21.05.0

ENV TEST_TOOLS /builds/worker/test-tools
ENV PATH ${PATH}:${TEST_TOOLS}:${TEST_TOOLS}/google-cloud-sdk/bin

RUN mkdir -p ${TEST_TOOLS} && \
    mkdir -p ${HOME}/.config/gcloud

RUN curl https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-${GOOGLE_SDK_VERSION}.0.0-linux-x86_64.tar.gz --output ${GOOGLE_SDK_DOWNLOAD} \
    && tar -xvf ${GOOGLE_SDK_DOWNLOAD} -C ${TEST_TOOLS} \
    && rm -f ${GOOGLE_SDK_DOWNLOAD} \
    && ${TEST_TOOLS}/google-cloud-sdk/install.sh --quiet \
    && ${TEST_TOOLS}/google-cloud-sdk/bin/gcloud --quiet components update

RUN URL_FLANK_BIN=$(curl -s "https://api.github.com/repos/Flank/flank/releases" | grep "browser_download_url*" | grep "${FLANK_VERSION}" | sed -r "s/\"//g" | cut -d ":" -f3) \
    && wget "https:${URL_FLANK_BIN}" -O ${TEST_TOOLS}/flank.jar \
    && chmod +x ${TEST_TOOLS}/flank.jar


# run-task expects to run as root
USER root
