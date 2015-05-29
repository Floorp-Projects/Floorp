Protocol Buffers (protobuf) source is available (via svn) at:

    svn checkout http://protobuf.googlecode.com/svn/trunk/ protobuf-read-only

Or via git at:

    https://github.com/google/protobuf

This code is covered under the BSD license (see COPYING.txt). Documentation is
available at http://code.google.com/p/protobuf.

The tree's current version of the protobuf library is 2.6.1.

We do not include the protobuf tests or the protoc compiler.

--------------------------------------------------------------------------------

# Upgrading the Protobuf Library

1. Get a new protobuf release from https://github.com/google/protobuf/releases

2. Run `$ ./toolkit/components/protobuf/upgrade_protobuf.sh ~/path/to/release/checkout/of/protobuf`.

3. Update the moz.build to export the new set of headers and add any new .cc
   files to the unified sources and remove old ones.
