# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['lib_core_neon_offsets.gypi'],
  'targets' : [
    {
      'target_name': 'gen_nsx_core_neon_offsets_h',
      'type': 'none',
      'dependencies': [
        'lib_core_neon_offsets',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx_obj_int_extract#host',
      ],
      'sources': ['<(shared_generated_dir)/nsx_core_neon_offsets.o',],
      'variables' : {
        'unpack_lib_name':'nsx_core_neon_offsets.o',
      },
      'includes': [
        '../../../third_party/libvpx/unpack_lib_posix.gypi',
        '../../../third_party/libvpx/obj_int_extract.gypi',
      ],
    },
    {
      'target_name': 'gen_aecm_core_neon_offsets_h',
      'type': 'none',
      'dependencies': [
        'lib_core_neon_offsets',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx_obj_int_extract#host',
      ],
      'variables': {
        'unpack_lib_name':'aecm_core_neon_offsets.o',
      },
      'sources': ['<(shared_generated_dir)/aecm_core_neon_offsets.o',],
      'includes': [
        '../../../third_party/libvpx/unpack_lib_posix.gypi',
        '../../../third_party/libvpx/obj_int_extract.gypi',
      ],
    },
  ],
}
