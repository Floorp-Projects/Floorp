Protocol Buffers (protobuf) source is available at:

    https://github.com/google/protobuf

This code is covered under the BSD license (see COPYING.txt). Documentation is
available at https://developers.google.com/protocol-buffers/.

The tree's current version of the protobuf library is 3.4.1.

We do not include the protobuf tests or the protoc compiler.

--------------------------------------------------------------------------------

# Upgrading the Protobuf Library

1. Get a new protobuf release from https://github.com/google/protobuf/releases

2. Run `$ ./toolkit/components/protobuf/upgrade_protobuf.sh ~/path/to/release/checkout/of/protobuf`.

3. Update the moz.build to export the new set of headers and add any new .cc
   files to the unified sources and remove old ones. Note that we only
   need:
   - files contained in the `libprotobuf_lite_la_SOURCES` target
   (https://github.com/google/protobuf/blob/master/src/Makefile.am)
   - the header files they need
   - gzip streams (for devtools)

4. Re-generate all .pb.cc and .pb.h files using `$ ./toolkit/components/protobuf/regenerate_cpp_files.sh`.
