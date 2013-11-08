# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file exists so we can find the isolate.gypi both when WebRTC is built
# stand-alone and when built as a part of Chrome.
# This is needed since GYP does not support evaluating variables in the
# includes sections of a target, so we cannot use <(DEPTH) or <(webrtc_root).
{
  'targets': [
    {
      'target_name': 'import_isolate_gypi',
      'type': 'none',
      'includes': [
        # Relative path to isolate.gypi when WebRTC built as a stand-alone
        # project (i.e. Chromium's build/ folder is checked out into the root).
        '../../build/isolate.gypi',
      ],
    },
  ],
}
