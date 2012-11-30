This library has been updated to protobuf-2.4.1 as of 11/30/12.

Protocol Buffers (protobuf) source is available (via svn) at:
svn checkout http://protobuf.googlecode.com/svn/trunk/ protobuf-read-only

This code is covered under the BSD license (see COPYING.txt). Documentation is
available at http://code.google.com/p/protobuf.

This import includes only files in protobuf-lite, a lighter-weight library that
does not support reflection or descriptors. Manual changes include removing all
tests, testdata, config.h, and all files not used in protobuf-lite.
