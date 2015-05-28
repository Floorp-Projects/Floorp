#!/usr/bin/env bash
##
# Install compiler hacks, necessary for builds to work properly,
# but ultimately to be removed in favor of something cleaner
##

# On ubuntu, the compiler that pip detects (x86_64-linux-gnu-gcc) is not
# available in the tooltool compiler, so we end up using the system default;
# to get around this, we link to the tooltool compiler that we'd prefer to use.
mv /usr/bin/x86_64-linux-gnu-gcc /usr/bin/x86_64-linux-gnu-gcc.orig
ln -s /home/worker/workspace/build/src/gcc/bin/gcc /usr/bin/x86_64-linux-gnu-gcc

# a.out.h needs to exist one directory lower, or the compiler will not find it
ln -s /usr/include/linux/a.out.h /usr/include/a.out.h

# Without this, zlib.h can't find zconf.h, so hey, symlinks to the rescue, right?
ln -s /usr/include/x86_64-linux-gnu/zconf.h /usr/include/zconf.h
