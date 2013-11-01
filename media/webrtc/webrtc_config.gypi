# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# definitions to control what gets built in webrtc
# NOTE!!! if you change something here, due to .gyp files not
# being reprocessed on .gypi changes, run this before building:
# "find . -name '*.gyp' | xargs touch"
{
  'variables': {
    'build_with_mozilla': 1,
    'build_with_chromium': 0,
    # basic stuff for everything
    'include_internal_video_render': 0,
    'clang_use_chrome_plugins': 0,
    'enable_protobuf': 0,
    'include_tests': 0,
    'enable_android_opensl': 1,
# use_system_lib* still seems to be in use in trunk/build
    'use_system_libjpeg': 0,
    'use_system_libvpx': 0,
    'build_libjpeg': 0,
    'build_libvpx': 0,
    # saves 4MB when webrtc_trace is off
    'enable_lazy_trace_alloc': 1,

    # turn off mandatory use of NEON and instead use NEON detection
    'arm_neon': 0,

    #if "-D build_with_gonk=1", then set moz_widget_toolkit_gonk to 1
    'moz_widget_toolkit_gonk': 0,
    'variables': {
      'build_with_gonk%': 0,
    },
    'conditions': [
      ['build_with_gonk==1', {
         'moz_widget_toolkit_gonk': 1,
      }],
    ],
# (for vp8) chromium sets to 0 also
    'use_temporal_layers': 0,
# Creates AEC internal sample dump files in current directory
#    'aec_debug_dump': 1,

    # codec enable/disables:
    # Note: if you change one here, you must modify layout/media/webrtc/Makefile.in!
    'include_g711': 1,
    'include_opus': 1,
    'include_g722': 0,
    'include_ilbc': 0,
    'include_isac': 0,
    'include_pcm16b': 1,
  }
}
