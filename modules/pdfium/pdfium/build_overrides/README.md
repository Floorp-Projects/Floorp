# Build overrides in GN

This directory is used to allow different products to customize settings
for repos which are DEPS'ed in or shared.

For example: V8 can be built on its own (in a "standalone" configuration),
and it can be built as part of Chromium. V8 defines a top-level
target, //v8:d8 (a simple executable), which will only be built in the
standalone configuration. To indiate itis a standalone configuration, v8 can
create a file, build_overrides/v8.gni, containing a variable,
`build_standalone_d8 = true` and import it (as
import("//build_overrides/v8.gni") from its top-level BUILD.gn file.

Chromium, on the other hand, does not need to build d8, and so it would
create its own build_overrides/v8.gni file, and in it set
`build_standalone_d8 = false`.

The two files should define the same set of variables, but the values may
vary as appropriate to suit the the needs of the two different builds.

The build.gni file provides a way for projects to override defaults for
variables used in //build itself (which we want to be shareable between
projects).

TODO(crbug.com/588513): Ideally //build_overrides and, in particular,
//build_overrides/build.gni will go away completely in favor of some
mechanism that can re-use other required files like //.gn, so that we don't
have to keep requiring projects to create a bunch of different files to use GN.
