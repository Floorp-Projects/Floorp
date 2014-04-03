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
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '.',
        '../interface',
        'include',
        'dummy', # dummy audio device
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
      ],
      'conditions': [
        ['build_with_mozilla==1', {
          'cflags_mozilla': [
            '$(NSPR_CFLAGS)',
          ],
        }],
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
        ['moz_widget_toolkit_gonk==1', {
          'cflags_mozilla': [
            '-I$(ANDROID_SOURCE)/frameworks/wilhelm/include',
            '-I$(ANDROID_SOURCE)/system/media/wilhelm/include',
          ],
          'include_dirs': [
            'android',
          ],
        }], # moz_widget_toolkit_gonk==1
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
        ['include_internal_audio_device==1', {
          'sources': [
            'linux/audio_device_utility_linux.cc',
            'linux/audio_device_utility_linux.h',
            'linux/latebindingsymboltable_linux.cc',
            'linux/latebindingsymboltable_linux.h',
            'ios/audio_device_ios.cc',
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
            # used externally for getUserMedia
            'opensl/single_rw_fifo.cc',
            'opensl/single_rw_fifo.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'opensl/audio_manager_jni.cc',
                'opensl/audio_manager_jni.h',
                'android/audio_device_jni_android.cc',
                'android/audio_device_jni_android.h',
               ],
            }],
            ['OS=="android" or moz_widget_toolkit_gonk==1', {
              'link_settings': {
                'libraries': [
                  '-llog',
                  '-lOpenSLES',
                ],
              },
              'conditions': [
                ['enable_android_opensl==1', {
                  'sources': [
                    'opensl/audio_device_opensles.cc',
                    'opensl/audio_device_opensles.h',
                    'opensl/fine_audio_buffer.cc',
                    'opensl/fine_audio_buffer.h',
                    'opensl/low_latency_event_posix.cc',
                    'opensl/low_latency_event.h',
                    'opensl/opensles_common.cc',
                    'opensl/opensles_common.h',
                    'opensl/opensles_input.cc',
                    'opensl/opensles_input.h',
                    'opensl/opensles_output.h',
                    'shared/audio_device_utility_shared.cc',
                    'shared/audio_device_utility_shared.h',
                  ],
                }, {
                  'sources': [
                    'shared/audio_device_utility_shared.cc',
                    'shared/audio_device_utility_shared.h',
                    'android/audio_device_jni_android.cc',
                    'android/audio_device_jni_android.h',
                  ],
                }],
                ['enable_android_opensl_output==1', {
                  'sources': [
                    'opensl/opensles_output.cc'
                  ],
                  'defines': [
                    'WEBRTC_ANDROID_OPENSLES_OUTPUT',
                  ]},
                ],
              ],
            }],
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-ldl','-lX11',
                ],
              },
            }],
            ['include_alsa_audio==1', {
              'cflags_mozilla': [
                '$(MOZ_ALSA_CFLAGS)',
              ],
              'defines': [
                'LINUX_ALSA',
              ],
              'sources': [
                'linux/alsasymboltable_linux.cc',
                'linux/alsasymboltable_linux.h',
                'linux/audio_device_alsa_linux.cc',
                'linux/audio_device_alsa_linux.h',
                'linux/audio_mixer_manager_alsa_linux.cc',
                'linux/audio_mixer_manager_alsa_linux.h',
              ],
            }],
            ['include_pulse_audio==1', {
              'cflags_mozilla': [
                '$(MOZ_PULSEAUDIO_CFLAGS)',
              ],
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
            ['OS=="mac" or OS=="ios"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
                  '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
                ],
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
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
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
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
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
                '<(import_isolate_path):import_isolate_gypi',
                'audio_device_tests',
              ],
              'includes': [
                'audio_device_tests.isolate',
              ],
              'sources': [
                'audio_device_tests.isolate',
              ],
            },
          ],
        }],
        ['OS=="android" and enable_android_opensl==1', {
          'targets': [
            {
              'target_name': 'audio_device_unittest',
              'type': 'executable',
              'dependencies': [
                'audio_device',
                'webrtc_utility',
                '<(DEPTH)/testing/gmock.gyp:gmock',
                '<(DEPTH)/testing/gtest.gyp:gtest',
                '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
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
