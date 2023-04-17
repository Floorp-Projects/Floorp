# Building

Marionette is built into Firefox by default and ships in the official
Firefox binary.  As Marionette is written in [XPCOM] flavoured
JavaScript, you may choose to rely on so called [artifact builds],
which will download pre-compiled Firefox blobs to your computer.
This means you don’t have to compile Firefox locally, but does
come at the cost of having a good internet connection.  To enable
[artifact builds] you may choose ‘Firefox for Desktop Artifact
Mode’ when bootstrapping.

Once you have a clone of [mozilla-unified], you can set up your
development environment by running this command and following the
on-screen instructions:

```shell
./mach bootstrap
```

When you're getting asked to choose the version of Firefox you want to build,
you may want to consider choosing "Firefox for Desktop Artifact Mode".  This
significantly reduces the time it takes to build Firefox on your machine
(from 30+ minutes to just 1-2 minutes) if you have a fast internet connection.

To perform a regular build, simply do:

```shell
./mach build
```

You can clean out the objdir using this command:

```shell
./mach clobber
```

Occasionally a clean build will be required after you fetch the
latest changes from mozilla-central.  You will find that the the
build will error when this is the case.  To automatically do clean
builds when this happens you may optionally add this line to the
[mozconfig] file in your top source directory:

```
mk_add_options AUTOCLOBBER=1
```

If you compile Firefox frequently you will also want to enable
[ccache] and [sccache] if you develop on a macOS or Linux system:

```
mk_add_options 'export RUSTC_WRAPPER=sccache'
mk_add_options 'export CCACHE_CPP2=yes'
ac_add_options --with-ccache
```

You may also opt out of building all the WebDriver specific components
(Marionette, and the [Remote Agent]) by setting the following flag:

```
ac_add_options --disable-webdriver
```

[mozilla-unified]: https://mozilla-version-control-tools.readthedocs.io/en/latest/hgmozilla/unifiedrepo.html
[artifact builds]: /contributing/build/artifact_builds.rst
[mozconfig]: /build/buildsystem/mozconfigs.rst
[ccache]: https://ccache.samba.org/
[sccache]: https://github.com/mozilla/sccache
[Remote Agent]: /remote/index.rst
