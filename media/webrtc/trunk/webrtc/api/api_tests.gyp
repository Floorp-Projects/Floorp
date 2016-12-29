# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['OS=="ios"', {
      'targets': [
        {
          'target_name': 'rtc_api_objc_test',
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/api/api.gyp:rtc_api_objc',
            '<(webrtc_root)/base/base_tests.gyp:rtc_base_tests_utils',
          ],
          'sources': [
            'objctests/RTCIceCandidateTest.mm',
            'objctests/RTCIceServerTest.mm',
            'objctests/RTCMediaConstraintsTest.mm',
            'objctests/RTCSessionDescriptionTest.mm',
          ],
          'xcode_settings': {
            'CLANG_ENABLE_OBJC_ARC': 'YES',
            'CLANG_WARN_OBJC_MISSING_PROPERTY_SYNTHESIS': 'YES',
            'GCC_PREFIX_HEADER': 'objc/WebRTC-Prefix.pch',
            # |-ObjC| flag needed to make sure category method implementations
            # are included:
            # https://developer.apple.com/library/mac/qa/qa1490/_index.html
            'OTHER_LDFLAGS': ['-ObjC'],
          },
        }
      ],
    }], # OS=="ios"
  ],
}
