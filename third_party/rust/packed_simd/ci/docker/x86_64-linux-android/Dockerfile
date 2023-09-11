FROM ubuntu:20.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
  ca-certificates \
  curl \
  gcc \
  libc-dev \
  python \
  unzip \
  file \
  make

WORKDIR /android/
ENV ANDROID_ARCH=x86_64
COPY android-install-ndk.sh /android/
RUN sh /android/android-install-ndk.sh

ENV STDARCH_ASSERT_INSTR_LIMIT=30

# We do not run x86_64-linux-android tests on an android emulator.
# See ci/android-sysimage.sh for informations about how tests are run.
COPY android-sysimage.sh /android/
RUN bash /android/android-sysimage.sh x86_64 x86_64-24_r07.zip

ENV PATH=$PATH:/rust/bin:/android/ndk/toolchains/llvm/prebuilt/linux-x86_64/bin \
    CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER=x86_64-linux-android21-clang \
    CC_x86_64_linux_android=x86_64-linux-android21-clang \
    CXX_x86_64_linux_android=x86_64-linux-android21-clang++ \
    OBJDUMP=llvm-objdump \
    HOME=/tmp
