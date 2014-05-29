#!/usr/bin/env python
#
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This empty Python module is needed to avoid an error when importing
# gyp_chromium in gyp_webrtc. The reason we're doing that is to avoid code
# duplication. We don't use the functions that actually use the find_depot_tools
# module. It was introduced as a dependency in http://crrev.com/245412.
