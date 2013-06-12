# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file is meant to be included into a target to provide an action
# to generate C header files. These headers include definitions
# that can be used in ARM assembly files.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_asm_headers_lib',
#   'type': 'static_library',
#   'sources': [
#     'foo.c',
#     'bar.c',
#   ],
#   'includes': ['path/to/this/gypi/file'],
# }
#
# The headers are guaranteed to be generated before any
# source files, even within this target, are compiled.
#
# The 'asm_header_dir' variable specifies the path suffix that output
# files are generated under.

# TODO(kma): port this block from Android into other build systems.
{
  'variables': {
    'out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(asm_header_dir)',
    'process_outputs_as_sources': 1,
  },
  'rules': [
    {
      'rule_name': 'generate_asm_header',
      'extension': 'c',
      'inputs': [
        'generate_asm_header.py',
      ],
      'outputs': [
        '<(out_dir)/<(RULE_INPUT_ROOT).h',
      ],
      'action': [
        'python',
        '<(webrtc_root)/build/generate_asm_header.py',
        '--compiler=<!(/bin/echo -n ${ANDROID_GOMA_WRAPPER} '
          '<(android_toolchain)/*-gcc)',
        '--options=-I.. -I<@(android_ndk_include) -S',  # Compiler options.
        '--dir=<(out_dir)',
        '<(RULE_INPUT_PATH)',
      ],
      'message': 'Generating assembly header files',
      'process_outputs_as_sources': 1,
    },
  ],
  'direct_dependent_settings': {
    'include_dirs': ['<(out_dir)',],
  },
  # This target exports a hard dependency because it generates header files.
  'hard_dependency': 1,
}
