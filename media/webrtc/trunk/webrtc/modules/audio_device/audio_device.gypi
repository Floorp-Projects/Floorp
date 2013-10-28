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
        'dummy/audio_device_dummy.h',
        'dummy/audio_device_utility_dummy.h',
      ],
      'conditions': [
        ['build_with_mozilla==1', {
          'include_dirs': [
            '$(DIST)/include',
          ],
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
            '$(topsrcdir)/widget/android',
            'android',
          ],
        }], # OS==android
        ['moz_widget_toolkit_gonk==1', {
          'include_dirs': [
            '$(ANDROID_SOURCE)/frameworks/wilhelm/include',
            '$(ANDROID_SOURCE)/system/media/wilhelm/include',
          ],
        }], # moz_widget_toolkit_gonk==1
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
            'android/audio_device_utility_android.cc',
            'android/audio_device_utility_android.h',
# opensles is shared with gonk, so isn't here
            'android/audio_device_jni_android.cc',
            'android/audio_device_jni_android.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'sources': [
                'audio_device_opensles.cc',
                'audio_device_opensles.h',
              ],
              'link_settings': {
                'libraries': [
                  '-llog',
                  '-lOpenSLES',
                ],
              },
            }],
            ['moz_widget_toolkit_gonk==1', {
              'sources': [
                'audio_device_opensles.cc',
                'audio_device_opensles.h',
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
          'target_name': 'audio_device_integrationtests',
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
      ],
    }],
  ],
}

