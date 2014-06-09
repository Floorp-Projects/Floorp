# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file has common information for gen_core_neon_offsets.gyp
# and gen_core_neon_offsets_chromium.gyp
{
  'variables': {
    'variables' : {
      'lib_intermediate_name': '',
      'conditions' : [
        ['android_webview_build==1', {
          'lib_intermediate_name' : '<(android_src)/$(call intermediates-dir-for, STATIC_LIBRARIES, lib_core_neon_offsets)/lib_core_neon_offsets.a',
        }],
      ],
    },
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/audio_processing/asm_offsets',
    'output_dir': '<(shared_generated_dir)',
    'output_format': 'cheader',
    'unpack_lib_search_path_list': [
      '-a', '<(PRODUCT_DIR)/lib_core_neon_offsets.a',
      '-a', '<(LIB_DIR)/webrtc/modules/audio_processing/lib_core_neon_offsets.a',
      '-a', '<(LIB_DIR)/third_party/webrtc/modules/audio_processing/lib_core_neon_offsets.a',
      '-a', '<(lib_intermediate_name)',
    ],
    'unpack_lib_output_dir':'<(shared_generated_dir)',
  },
  'includes': [
    '../../build/common.gypi',
  ],
  'conditions': [
    ['((target_arch=="arm" and arm_version==7) or target_arch=="armv7") and (OS=="android" or OS=="ios")', {
      'targets' : [
        {
          'target_name': 'lib_core_neon_offsets',
          'type': 'static_library',
          'android_unmangled_name': 1,
          'hard_dependency': 1,
          'sources': [
            'ns/nsx_core_neon_offsets.c',
            'aecm/aecm_core_neon_offsets.c',
          ],
        },
      ],
    }],
  ],
}
