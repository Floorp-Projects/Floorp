# Building

The Remote Agent is included in the default Firefox build, but only
ships on the Firefox Nightly release channel:

```shell
% ./mach run --remote-debugging-port
```

The source code can be found under [remote/ in central].

There are two build modes to choose from:

## Full build mode

The Remote Agent is included when you build in the usual way:

```shell
% ./mach build
```

When you make changes to XPCOM component files you need to rebuild
in order for the changes to take effect.  The most efficient way to
do this, provided you haven’t touched any compiled code (C++ or Rust):

```shell
% ./mach build faster
```

Component files include the likes of components.conf,
RemoteAgent.manifest, moz.build files, and jar.mn.
All the JS modules (files ending with `.jsm`) are symlinked into
the build and can be changed without rebuilding.

You may also opt out of building all the WebDriver specific components
([Marionette], and the Remote Agent) by setting the following flag in
your [mozconfig]:

```make
ac_add_options --disable-webdriver
```

## Artifact mode

You may also use [artifact builds] when working on the Remote Agent.
This fast build mode downloads pre-built components from the Mozilla
build servers, rendering local compilation unnecessary.  To use
them, place this in your [mozconfig]:

```make
ac_add_options --enable-artifact-builds
```

[remote/ in central]: https://searchfox.org/mozilla-central/source/remote
[mozconfig]: /build/buildsystem/mozconfigs.rst
[artifact builds]: /contributing/build/artifact_builds.rst
[Marionette]: /testing/marionette/index.rst
