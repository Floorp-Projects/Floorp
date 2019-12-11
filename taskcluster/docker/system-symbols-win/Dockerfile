FROM python:3.7-slim

RUN useradd -d /home/worker -s /bin/bash -m worker
WORKDIR /home/worker
RUN chown -R worker.worker /home/worker/
RUN mkdir dump_syms
COPY requirements.txt /setup/requirements.txt

RUN apt-get update \
    && apt-get install -y --no-install-recommends wget git \
    && pip install --disable-pip-version-check --quiet --no-cache-dir -r /setup/requirements.txt \
    && wget -qO- https://github.com/mozilla/dump_syms/releases/latest/download/dump_syms-linux-x86_64.tar.gz | tar xvz -C dump_syms \
    && apt-get remove -y wget \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/*

ENV DUMP_SYMS_PATH /home/worker/dump_syms/dump_syms-linux-x86_64/dump_syms


# %include tools/crashreporter/system-symbols/
USER worker
WORKDIR /home/worker/
COPY topsrcdir/tools/crashreporter/system-symbols/win /home/worker/win-update-symbols
CMD ["win-update-symbols/run.sh"]
