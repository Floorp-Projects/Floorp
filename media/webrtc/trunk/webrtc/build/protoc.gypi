# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# It was necessary to copy this file to WebRTC, because the path to
# build/common.gypi is different for the standalone and Chromium builds. Gyp
# doesn't permit conditional inclusion or variable expansion in include paths.
# http://code.google.com/p/gyp/wiki/InputFormatReference#Including_Other_Files

# This file is meant to be included into a target to provide a rule
# to invoke protoc in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_proto_lib',
#   'type': 'static_library',
#   'sources': [
#     'foo.proto',
#     'bar.proto',
#   ],
#   'variables': {
#     # Optional, see below: 'proto_in_dir': '.'
#     'proto_out_dir': 'dir/for/my_proto_lib'
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
# If necessary, you may add normal .cc files to the sources list or other gyp
# dependencies.  The proto headers are guaranteed to be generated before any
# source files, even within this target, are compiled.
#
# The 'proto_in_dir' variable must be the relative path to the
# directory containing the .proto files.  If left out, it defaults to '.'.
#
# The 'proto_out_dir' variable specifies the path suffix that output
# files are generated under.  Targets that gyp-depend on my_proto_lib
# will be able to include the resulting proto headers with an include
# like:
#   #include "dir/for/my_proto_lib/foo.pb.h"
#
# Implementation notes:
# A proto_out_dir of foo/bar produces
#   <(SHARED_INTERMEDIATE_DIR)/protoc_out/foo/bar/{file1,file2}.pb.{cc,h}
#   <(SHARED_INTERMEDIATE_DIR)/pyproto/foo/bar/{file1,file2}_pb2.py

{
  'variables': {
    'protoc': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'cc_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/<(proto_out_dir)',
    'py_dir': '<(PRODUCT_DIR)/pyproto/<(proto_out_dir)',
    'proto_in_dir%': '.',
  },
  'rules': [
    {
      'rule_name': 'genproto',
      'extension': 'proto',
      'inputs': [
        '<(protoc)',
      ],
      'outputs': [
        '<(py_dir)/<(RULE_INPUT_ROOT)_pb2.py',
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.cc',
        '<(cc_dir)/<(RULE_INPUT_ROOT).pb.h',
      ],
      'action': [
        '<(protoc)',
        '--proto_path=<(proto_in_dir)',
        # Naively you'd use <(RULE_INPUT_PATH) here, but protoc requires
        # --proto_path is a strict prefix of the path given as an argument.
        '<(proto_in_dir)/<(RULE_INPUT_ROOT)<(RULE_INPUT_EXT)',
        '--cpp_out=<(cc_dir)',
        '--python_out=<(py_dir)',
        ],
      'message': 'Generating C++ and Python code from <(RULE_INPUT_PATH)',
      'process_outputs_as_sources': 1,
    },
  ],
  'dependencies': [
    '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
    '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
  ],
  'include_dirs': [
    '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
  ],
  'direct_dependent_settings': {
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    ]
  },
  'export_dependent_settings': [
    # The generated headers reference headers within protobuf_lite,
    # so dependencies must be able to find those headers too.
    '<(DEPTH)/third_party/protobuf/protobuf.gyp:protobuf_lite',
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
