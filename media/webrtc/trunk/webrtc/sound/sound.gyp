# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      'target_name': 'rtc_sound',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base',
      ],
      'sources': [
        'automaticallychosensoundsystem.h',
        'nullsoundsystem.cc',
        'nullsoundsystem.h',
        'nullsoundsystemfactory.cc',
        'nullsoundsystemfactory.h',
        'platformsoundsystem.cc',
        'platformsoundsystem.h',
        'platformsoundsystemfactory.cc',
        'platformsoundsystemfactory.h',
        'sounddevicelocator.h',
        'soundinputstreaminterface.h',
        'soundoutputstreaminterface.h',
        'soundsystemfactory.h',
        'soundsysteminterface.cc',
        'soundsysteminterface.h',
        'soundsystemproxy.cc',
        'soundsystemproxy.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'alsasoundsystem.cc',
            'alsasoundsystem.h',
            'alsasymboltable.cc',
            'alsasymboltable.h',
            'linuxsoundsystem.cc',
            'linuxsoundsystem.h',
            'pulseaudiosoundsystem.cc',
            'pulseaudiosoundsystem.h',
            'pulseaudiosymboltable.cc',
            'pulseaudiosymboltable.h',
          ],
        }],
      ],
    },
  ],
}
