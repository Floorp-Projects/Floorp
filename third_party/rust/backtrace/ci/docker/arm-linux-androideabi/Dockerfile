FROM ubuntu:18.04

RUN apt-get update && apt-get install -y --no-install-recommends \
  curl \
  ca-certificates \
  unzip \
  openjdk-8-jre \
  python \
  gcc \
  libc6-dev

COPY android-ndk.sh /
RUN /android-ndk.sh arm
WORKDIR /android
COPY android-sdk.sh /android/sdk.sh
RUN ./sdk.sh arm
RUN mv /root/.android /tmp
RUN chmod 777 -R /tmp/.android
RUN chmod 755 /android/sdk/tools/* /android/sdk/emulator/qemu/linux-x86_64/*
ENV PATH=$PATH:/android-toolchain/bin:/android/sdk/platform-tools

# TODO: run tests in an emulator eventually
ENV CARGO_TARGET_ARM_LINUX_ANDROIDEABI_LINKER=arm-linux-androideabi-gcc \
    CARGO_TARGET_ARM_LINUX_ANDROIDEABI_RUNNER=/tmp/runtest \
    HOME=/tmp

ADD runtest-android.rs /tmp/runtest.rs
ENTRYPOINT [ \
  "bash", \
  "-c", \
  # set SHELL so android can detect a 64bits system, see
  # http://stackoverflow.com/a/41789144
  "SHELL=/bin/dash /android/sdk/emulator/emulator @arm -no-window & \
   /rust/bin/rustc /tmp/runtest.rs -o /tmp/runtest && \
   exec \"$@\"", \
  "--" \
]
