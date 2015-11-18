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
        '../interface',
        'include',
        'dummy',  # Contains dummy audio device implementations.
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../interface',
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
        'audio_device_utility.cc',
        'audio_device_utility.h',
        'audio_device_impl.cc',
        'audio_device_impl.h',
        'audio_device_config.h',
        'dummy/audio_device_dummy.cc',
        'dummy/audio_device_dummy.h',
        'dummy/audio_device_utility_dummy.cc',
        'dummy/audio_device_utility_dummy.h',
        'dummy/file_audio_device.cc',
        'dummy/file_audio_device.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'include_dirs': [
            'linux',
          ],
        }], # OS==linux
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
            'android',
          ],
        }], # OS==android
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
            'linux/alsasymboltable_linux.cc',
            'linux/alsasymboltable_linux.h',
            'linux/audio_device_alsa_linux.cc',
            'linux/audio_device_alsa_linux.h',
            'linux/audio_device_utility_linux.cc',
            'linux/audio_device_utility_linux.h',
            'linux/audio_mixer_manager_alsa_linux.cc',
            'linux/audio_mixer_manager_alsa_linux.h',
            'linux/latebindingsymboltable_linux.cc',
            'linux/latebindingsymboltable_linux.h',
            'ios/audio_device_ios.mm',
            'ios/audio_device_ios.h',
            'ios/audio_device_utility_ios.cc',
            'ios/audio_device_utility_ios.h',
            'mac/audio_device_mac.cc',
            'mac/audio_device_mac.h',
            'mac/audio_device_utility_mac.cc',
            'mac/audio_device_utility_mac.h',
            'mac/audio_mixer_manager_mac.cc',
            'mac/audio_mixer_manager_mac.h',
            'mac/portaudio/pa_memorybarrier.h',
            'mac/portaudio/pa_ringbuffer.c',
            'mac/portaudio/pa_ringbuffer.h',
            'win/audio_device_core_win.cc',
            'win/audio_device_core_win.h',
            'win/audio_device_wave_win.cc',
            'win/audio_device_wave_win.h',
            'win/audio_device_utility_win.cc',
            'win/audio_device_utility_win.h',
            'win/audio_mixer_manager_win.cc',
            'win/audio_mixer_manager_win.h',
            'android/audio_device_template.h',
            'android/audio_device_utility_android.cc',
            'android/audio_device_utility_android.h',
            'android/audio_manager.cc',
            'android/audio_manager.h',
            'android/audio_manager_jni.cc',
            'android/audio_manager_jni.h',
            'android/audio_record_jni.cc',
            'android/audio_record_jni.h',
            'android/audio_track_jni.cc',
            'android/audio_track_jni.h',
            'android/fine_audio_buffer.cc',
            'android/fine_audio_buffer.h',
            'android/low_latency_event_posix.cc',
            'android/low_latency_event.h',
            'android/opensles_common.cc',
            'android/opensles_common.h',
            'android/opensles_input.cc',
            'android/opensles_input.h',
            'android/opensles_output.cc',
            'android/opensles_output.h',
            'android/single_rw_fifo.cc',
            'android/single_rw_fifo.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'link_settings': {
                'libraries': [
                  '-llog',
                  '-lOpenSLES',
                ],
              },
            }],
            ['OS=="linux"', {
              'defines': [
                'LINUX_ALSA',
              ],
              'link_settings': {
                'libraries': [
                  '-ldl','-lX11',
                ],
              },
              'conditions': [
                ['include_pulse_audio==1', {
                  'defines': [
                    'LINUX_PULSE',
                  ],
                  'sources': [
                    'linux/audio_device_pulse_linux.cc',
                    'linux/audio_device_pulse_linux.h',
                    'linux/audio_mixer_manager_pulse_linux.cc',
                    'linux/audio_mixer_manager_pulse_linux.h',
                    'linux/pulseaudiosymboltable_linux.cc',
                    'linux/pulseaudiosymboltable_linux.h',
                  ],
                }],
              ],
            }],
            ['OS=="mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
                  '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
                ],
              },
            }],
            ['OS=="ios"', {
              'xcode_settings': {
                'CLANG_ENABLE_OBJC_ARC': 'YES',
              },
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework AudioToolbox',
                    '-framework AVFoundation',
                    '-framework Foundation',
                  ],
                },
              },
            }],
            ['OS=="win"', {
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
          ], # conditions
        }], # include_internal_audio_device==1
      ], # conditions
    },
  ],
  'conditions': [
    ['include_tests==1', {
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
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'audio_device_tests_run',
              'type': 'none',
              'dependencies': [
                'audio_device_tests',
              ],
              'includes': [
                '../../build/isolate.gypi',
              ],
              'sources': [
                'audio_device_tests.isolate',
              ],
            },
          ],
        }],
        ['OS=="android"', {
          'targets': [
            {
              'target_name': 'audio_device_unittest',
              'type': 'executable',
              'dependencies': [
                'audio_device',
                'webrtc_utility',
                '<(DEPTH)/testing/gmock.gyp:gmock',
                '<(DEPTH)/testing/gtest.gyp:gtest',
                '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
                '<(webrtc_root)/test/test.gyp:test_support_main',
              ],
              'sources': [
                'android/fine_audio_buffer_unittest.cc',
                'android/low_latency_event_unittest.cc',
                'android/single_rw_fifo_unittest.cc',
                'mock/mock_audio_device_buffer.h',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ],
}

