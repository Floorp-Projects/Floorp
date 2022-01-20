Cross compiling dbus
====================

The examples below all assume you're trying to compile for Raspberry Pi 2 or 3 running Raspbian. Adjust target triples accordingly if your target is something else.

Before you start: there is an alternative guide at [issue 184](https://github.com/diwic/dbus-rs/issues/184#issuecomment-520228758), which also contains a [powershell script](https://github.com/diwic/dbus-rs/issues/184#issuecomment-520791888) that set things up for you. Feel free to use either this guide or the alternative one as you see fit.

A cross linker
--------------

Apparently, `rustc` in itself can generate code for many archs but not assemble the generated code into the final executable. Hence you need a cross linker.

**Install it** - here follow whatever guide you have for the target arch. Distributions may also ship with cross toolchains. Example for Ubuntu 18.04:

`sudo apt install gcc-8-multilib-arm-linux-gnueabihf`

**Tell rustc where to find it** - in [.cargo/config](https://doc.rust-lang.org/cargo/reference/config.html) add the following:

```
[target.armv7-unknown-linux-gnueabihf]
linker = "arm-linux-gnueabihf-gcc-8"
```

Target rust std
---------------

This one's easy, just run rustup:

`rustup target add armv7-unknown-linux-gnueabihf`


Target dbus libraries
---------------------

**Installing the libraries**

Now to the more challenging part. Since we link to a C library `libdbus-1.so`, we also need the target version of that library. However, `libdbus-1.so` in itself links to a systemd library (at least it does so here) which in turn links to other libraries etc, so we need target versions of those libraries too.

Getting an entire rootfs/image is probably the easiest option. The rootfs needs to have `libdbus-1-dev` installed. I e:

 * Boot your target (i e, a raspberry), install `libdbus-1-dev` on it, turn it off and put the SD card into your computer's SD card reader. Mount the partition.
 * If the above is not an option, you could download an image, mount it at the right offset, like this (ugly hack!) `sudo mount -o loop,offset=50331648 2019-04-08-raspbian-stretch-lite.img /tmp/mnt` and then, to manually make the symlink that `libdbus-1-dev` does for you: `cd /tmp/mnt/usr/lib/arm-linux-gnueabihf && ln -s ../../../lib/arm-linux-gnueabihf/libdbus-1.so.3 libdbus-1.so`.
 * Or you can use the alternative guide's approach to download, extract and post-process the relevant `.deb` files manually. This might be a preferrable option if an entire image/rootfs would be too large.

**Finding the libraries**

When not cross compiling, finding the right library is done by a `build.rs` script which calls `pkg-config`. This will not work when cross compiling because it will point to the `libdbus-1.so` on the host, not the `libdbus-1.so` of the target.
Maybe it is possible to teach `pkg-config` how to return the target library instead, but I have not tried this approach. Instead we can override build script altogether and provide the same info manually. This is possible because `libdbus-sys` has a `links = dbus` line.

For the example below we assume that we have mounted a Raspbian rootfs on `/tmp/mnt`, and that the cross linker came with some basic libraries (libc, libpthread etc) that are installed on `/usr/arm-linux-gnueabihf/lib`. Unfortunately, we can't use the basic libraries that are present on the image, because they might contain absolute paths.

And so we add the following to [.cargo/config](https://doc.rust-lang.org/cargo/reference/config.html):

```
[target.armv7-unknown-linux-gnueabihf.dbus]
rustc-link-search = ["/usr/arm-linux-gnueabihf/lib", "/tmp/mnt/usr/lib/arm-linux-gnueabihf"]
rustc-link-lib = ["dbus-1"]
```


Finally
-------

If we are all set up, you should be able to successfully compile with:

`cargo build --target=armv7-unknown-linux-gnueabihf`


