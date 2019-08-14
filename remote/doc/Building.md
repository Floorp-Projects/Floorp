Building
========

The remote agent is included in the default Firefox build, but
disabled by default.  To expose the remote agent you can set the
`remote.enabled` preference to true before running it:

	% ./mach run --setpref="remote.enabled=true" --remote-debugger

The source code is found under `$(topsrcdir)/remote`.

Full build mode
---------------

The remote agent supports only Firefox, and is included when you
build in the usual way:

	% ./mach build

When you make changes to XPCOM component files you need to rebuild
in order for the changes to take effect.  The most efficient way to
do this, provided you haven’t touched any compiled code (C++ or Rust):

	% ./mach build faster

Component files include the likes of command-line-handler.js,
RemoteAgent.manifest, moz.build files, and jar.mn.
All the JS modules (files ending with `.jsm`) are symlinked into
the build and can be changed without rebuilding.

You may also opt out of building the remote agent entirely by setting
the `--disable-cdp` build flag in your [mozconfig]:

	ac_add_options --disable-cdp


Artifact mode
-------------

You may also use [artifact builds] when working on the remote agent.
This fast build mode downloads pre-built components from the Mozilla
build servers, rendering local compilation unnecessary.  To use
them, place this in your [mozconfig]:

	ac_add_options --enable-artifact-builds


[mozconfig]: ../build/buildsystem/mozconfigs.html
[artifact builds]: https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds
