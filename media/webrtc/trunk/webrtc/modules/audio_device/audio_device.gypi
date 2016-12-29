# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'audio_device',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '.',
        '../include',
        'include',
        'dummy',  # Contains dummy audio device implementations.
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../include',
          'include',
        ],
      },
      # TODO(xians): Rename files to e.g. *_linux.{ext}, remove sources in conditions section
      'sources': [
        'include/audio_device.h',
        'include/audio_device_defines.h',
        'audio_device_buffer.cc',
        'audio_device_buffer.h',
        'audio_device_generic.cc',
        'audio_device_generic.h',
        'audio_device_config.h',
        'dummy/audio_device_dummy.cc',
        'dummy/audio_device_dummy.h',
        'dummy/file_audio_device.cc',
        'dummy/file_audio_device.h',
        'fine_audio_buffer.cc',
        'fine_audio_buffer.h',
      ],
      'conditions': [
        ['build_with_mozilla==1', {
          'cflags_mozilla': [
            '$(NSPR_CFLAGS)',
          ],
        }],
        ['hardware_aec_ns==1', {
          'defines': [
            'WEBRTC_HARDWARE_AEC_NS',
          ],
        }],
        ['include_sndio_audio==1', {
          'include_dirs': [
            'sndio',
          ],
        }], # include_sndio_audio==1
        ['OS=="linux" or include_alsa_audio==1 or include_pulse_audio==1', {
          'include_dirs': [
            'linux',
          ],
        }], # OS=="linux" or include_alsa_audio==1 or include_pulse_audio==1
        ['OS=="ios"', {
          'include_dirs': [
            'ios',
          ],
        }], # OS==ios
        ['OS=="mac"', {
          'include_dirs': [
            'mac',
          ],
        }], # OS==mac
        ['OS=="win"', {
          'include_dirs': [
            'win',
          ],
        }],
        ['OS=="android"', {
          'include_dirs': [
            '/widget/android',
            'android',
          ],
        }], # OS==android
        ['enable_android_opensl==1', {
          'include_dirs': [
            'opensl',
          ],
        }], # enable_android_opensl
        ['include_internal_audio_device==0', {
          'defines': [
            'WEBRTC_DUMMY_AUDIO_BUILD',
          ],
        }],
        ['build_with_chromium==0', {
          'sources': [
            # Don't link these into Chrome since they contain static data.
            'dummy/file_audio_device_factory.cc',
            'dummy/file_audio_device_factory.h',
          ],
        }],
        ['include_internal_audio_device==1', {
          'sources': [
            'audio_device_impl.cc',
            'audio_device_impl.h',
            # used externally for getUserMedia
            'opensl/single_rw_fifo.cc',
            'opensl/single_rw_fifo.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'android/audio_device_template.h',
                'android/audio_manager.cc',
                'android/audio_manager.h',
                'android/audio_record_jni.cc',
                'android/audio_record_jni.h',
                'android/audio_track_jni.cc',
                'android/audio_track_jni.h',
                'android/build_info.cc',
                'android/build_info.h',
                'android/opensles_common.cc',
                'android/opensles_common.h',
                'android/opensles_player.cc',
                'android/opensles_player.h',
              ],
              'link_settings': {
                'libraries': [
                  '-llog',
                  '-lOpenSLES',
                ],
              },
            }],
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],
            ['include_sndio_audio==1', {
              'link_settings': {
                'libraries': [
                  '-lsndio',
                ],
              },
              'sources': [
                'sndio/audio_device_sndio.cc',
                'sndio/audio_device_sndio.h',
                'sndio/audio_device_utility_sndio.cc',
                'sndio/audio_device_utility_sndio.h',
              ],
            }],
            ['include_alsa_audio==1', {
              'cflags_mozilla': [
                '$(MOZ_ALSA_CFLAGS)',
              ],
              'defines': [
                'LINUX_ALSA',
              ],
              'link_settings': {
                'libraries': [
                  '-lX11',
                ],
              },
              'sources': [
                'linux/alsasymboltable_linux.cc',
                'linux/alsasymboltable_linux.h',
                'linux/audio_device_alsa_linux.cc',
                'linux/audio_device_alsa_linux.h',
                'linux/audio_mixer_manager_alsa_linux.cc',
                'linux/audio_mixer_manager_alsa_linux.h',
                'linux/latebindingsymboltable_linux.cc',
                'linux/latebindingsymboltable_linux.h',
              ],
            }],
            ['include_pulse_audio==1', {
              'cflags_mozilla': [
                '$(MOZ_PULSEAUDIO_CFLAGS)',
              ],
              'defines': [
                'LINUX_PULSE',
              ],
              'link_settings': {
                'libraries': [
                  '-lX11',
                ],
              },
              'sources': [
                'linux/audio_device_pulse_linux.cc',
                'linux/audio_device_pulse_linux.h',
                'linux/audio_mixer_manager_pulse_linux.cc',
                'linux/audio_mixer_manager_pulse_linux.h',
                'linux/latebindingsymboltable_linux.cc',
                'linux/latebindingsymboltable_linux.h',
                'linux/pulseaudiosymboltable_linux.cc',
                'linux/pulseaudiosymboltable_linux.h',
              ],
            }],
            ['OS=="mac"', {
              'sources': [
                'mac/audio_device_mac.cc',
                'mac/audio_device_mac.h',
                'mac/audio_mixer_manager_mac.cc',
                'mac/audio_mixer_manager_mac.h',
                'mac/portaudio/pa_memorybarrier.h',
                'mac/portaudio/pa_ringbuffer.c',
                'mac/portaudio/pa_ringbuffer.h',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
                  '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
                ],
              },
            }],
            ['OS=="ios"', {
              'sources': [
                'ios/audio_device_ios.h',
                'ios/audio_device_ios.mm',
                'ios/audio_device_not_implemented_ios.mm',
              ],
              'xcode_settings': {
                'CLANG_ENABLE_OBJC_ARC': 'YES',
              },
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework AudioToolbox',
                    '-framework AVFoundation',
                    '-framework Foundation',
                    '-framework UIKit',
                  ],
                },
              },
            }],
            ['OS=="win"', {
              'sources': [
                'win/audio_device_core_win.cc',
                'win/audio_device_core_win.h',
                'win/audio_device_wave_win.cc',
                'win/audio_device_wave_win.h',
                'win/audio_mixer_manager_win.cc',
                'win/audio_mixer_manager_win.h',
              ],
              'link_settings': {
                'libraries': [
                  # Required for the built-in WASAPI AEC.
                  '-ldmoguids.lib',
                  '-lwmcodecdspuuid.lib',
                  '-lamstrmid.lib',
                  '-lmsdmo.lib',
                ],
              },
            }],
            ['OS=="win" and clang==1', {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'AdditionalOptions': [
                    # Disable warnings failing when compiling with Clang on Windows.
                    # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                    '-Wno-bool-conversion',
                    '-Wno-delete-non-virtual-dtor',
                    '-Wno-logical-op-parentheses',
                    '-Wno-microsoft-extra-qualification',
                    '-Wno-microsoft-goto',
                    '-Wno-missing-braces',
                    '-Wno-parentheses-equality',
                    '-Wno-reorder',
                    '-Wno-shift-overflow',
                    '-Wno-tautological-compare',
                    '-Wno-unused-private-field',
                  ],
                },
              },
            }],
          ], # conditions
        }], # include_internal_audio_device==1
      ], # conditions
    },
  ],
  'conditions': [
    # Does not compile on iOS: webrtc:4755.
    ['include_tests==1 and OS!="ios"', {
      'targets': [
        {
          'target_name': 'audio_device_tests',
          'type': 'executable',
          'dependencies': [
            'audio_device',
            'webrtc_utility',
            '<(webrtc_root)/test/test.gyp:test_support_main',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
          ],
          'sources': [
            'test/audio_device_test_api.cc',
            'test/audio_device_test_defines.h',
          ],
        },
        {
          'target_name': 'audio_device_test_func',
          'type': 'executable',
          'dependencies': [
            'audio_device',
            'webrtc_utility',
            '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/audio_device_test_func.cc',
            'test/audio_device_test_defines.h',
            'test/func_test_manager.cc',
            'test/func_test_manager.h',
          ],
        },
      ], # targets
    }], # include_tests==1 and OS!=ios
  ],
}
