Building
========

The remote agent is by default not included in Firefox builds.
To build it, put this in your [mozconfig]:

	ac_add_options --enable-cdp

This exposes a `--remote-debugger` flag you can use to start the
remote agent:

	% ./mach run --setpref="remote.enabled=true" --remote-debugger

When you make changes to the XPCOM component you need to rebuild
in order for the changes to take effect.  The most efficient way to
do this, provided you haven’t touched any compiled code (C++ or Rust):

	% ./mach build faster

Component files include the likes of RemoteAgent.js, RemoteAgent.manifest,
moz.build files, prefs/remote.js, and jar.mn.  All the JS modules
(files ending with `.jsm`) are symlinked into the build and can be
changed without rebuilding.

[mozconfig]: ../build/buildsystem/mozconfigs.html
