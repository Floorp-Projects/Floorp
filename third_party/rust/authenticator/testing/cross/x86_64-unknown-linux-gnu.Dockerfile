FROM rustembedded/cross:x86_64-unknown-linux-gnu-0.2.1

RUN apt-get update && \
    apt-get install --assume-yes libudev-dev

RUN pkg-config --list-all && pkg-config --libs libudev && \
    pkg-config --modversion libudev