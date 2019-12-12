FROM ubuntu:16.04
RUN apt-get update && apt-get install -y git python curl pax gzip tar subversion autoconf build-essential libxml2-dev openssl libssl-dev make libz-dev libusb-dev cmake libbz2-dev libpng-dev wget virtualenv zip python-setuptools python-dev liblzma-dev
RUN easy_install pip && pip install futures
RUN useradd -d /home/worker -s /bin/bash -m worker
RUN mkdir /opt/data-reposado/
RUN mkdir /home/worker/bin/
RUN chown -R worker.worker /opt/data-reposado/ /home/worker/

COPY setup.sh /setup/
COPY requirements.txt /setup/
# %include tools/crashreporter/system-symbols/mac
COPY topsrcdir/tools/crashreporter/system-symbols/mac/repo_sync /setup/

RUN /bin/sh /setup/setup.sh

ADD https://github.com/marco-c/breakpad-mac-update-symbols/raw/21221733edfbcac49d40e50fc219fab7d17437a0/lipo /usr/local/bin/
RUN chmod +x /usr/local/bin/lipo

USER worker
WORKDIR /home/worker/
COPY topsrcdir/tools/crashreporter/system-symbols/mac /home/worker/mac-update-symbols
WORKDIR /home/worker/mac-update-symbols
CMD ["./run.sh"]
