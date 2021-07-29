Building
========

The Remote Agent is included in the default Firefox build, but only
ships on the Firefox Nightly release channel:

	% ./mach run --remote-debugging-port

The source code can be found under [remote/ in central].

There are two build modes to choose from:

Full build mode
---------------

The Remote Agent is included when you build in the usual way:

	% ./mach build

When you make changes to XPCOM component files you need to rebuild
in order for the changes to take effect.  The most efficient way to
do this, provided you haven’t touched any compiled code (C++ or Rust):

	% ./mach build faster

Component files include the likes of components.conf,
RemoteAgent.manifest, moz.build files, and jar.mn.
All the JS modules (files ending with `.jsm`) are symlinked into
the build and can be changed without rebuilding.
The Remote Agent’s startup code found under remote/components/rust/
is written in Rust and requires rebuilds when changed.

You may also opt out of building all the WebDriver specific components
([Marionette], and the Remote Agent) by setting the following flag in
your [mozconfig]:

    ac_add_options --disable-webdriver

Artifact mode
-------------

You may also use [artifact builds] when working on the Remote Agent.
This fast build mode downloads pre-built components from the Mozilla
build servers, rendering local compilation unnecessary.  To use
them, place this in your [mozconfig]:

	ac_add_options --enable-artifact-builds


[remote/ in central]: https://searchfox.org/mozilla-central/source/remote
[mozconfig]: ../build/buildsystem/mozconfigs.html
[artifact builds]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds
[Marionette]: ../testing/marionette/index.html
