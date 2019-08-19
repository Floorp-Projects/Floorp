Self-serving an ARM build
=========================

Mozilla [announced the intent] to deprecate ARMv7 HF builds of
geckodriver in September 2018.  This does not mean you can no longer
use geckodriver on ARM systems, and this document explains how you
can self-service a build for ARMv7 HF.

Assuming you have already checked out [central], the steps to
cross-compile ARMv7 from a Linux host system is as follows:

  1. If you don’t have Rust installed:

         # curl https://sh.rustup.rs -sSf | sh

  2. Install cross-compiler toolchain:

         # apt install gcc-arm-linux-gnueabihf libc6-armhf-cross libc6-dev-armhf-cross

  3. Createa a new shell, or to reuse the existing shell:

         source $HOME/.cargo/env

  4. Install rustc target toolchain:

         % rustup target install armv7-unknown-linux-gnueabihf

  5. Put this in testing/geckodriver/.cargo/config:

         [target.armv7-unknown-linux-gnueabihf]
         linker = "arm-linux-gnueabihf-gcc"

  6. Build geckodriver from testing/geckodriver:

         % cd testing/geckodriver
         % cargo build --release --target armv7-unknown-linux-gnueabihf

[announced the intent]: https://lists.mozilla.org/pipermail/tools-marionette/2018-September/000035.html
[central]: https://hg.mozilla.org/mozilla-central/
