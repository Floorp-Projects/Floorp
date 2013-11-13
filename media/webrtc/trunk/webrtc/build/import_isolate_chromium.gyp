# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# See webrtc/build/import_isolate_webrtc.gyp for information about this file.
{
  'targets': [
    {
      'target_name': 'import_isolate_gypi',
      'type': 'none',
      'includes': [
        # Relative path to isolate.gypi when WebRTC is built from inside
        # Chromium (i.e. the webrtc/ folder is checked out into third_party/).
        '../../../build/apk_test.gypi',
      ],
    },
  ],
}
