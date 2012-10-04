# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# definitions to control what gets built in webrtc
{
  'variables': {
    # basic stuff for everything
    'include_internal_video_render': 0,
    'clang_use_chrome_plugins': 0,
    'enable_protobuf': 0,
    'include_pulse_audio': 0,
    'include_tests': 0,
    'use_system_libjpeg': 1,
    'use_system_libvpx': 1,

    # codec enable/disables:
    # Note: if you change one here, you must modify shared_libs.mk!
    'codec_g711_enable': 1,
    'codec_opus_enable': 1,
    'codec_g722_enable': 0,
    'codec_ilbc_enable': 0,
    'codec_isac_enable': 0,
    'codec_pcm16b_enable': 1,
  }
}
