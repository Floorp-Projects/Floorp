# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    '../../build/common.gypi',
    'codecs/interfaces.gypi',
    'codecs/cng/cng.gypi',
    'codecs/g711/g711.gypi',
    'codecs/pcm16b/pcm16b.gypi',
    'codecs/red/red.gypi',
    'main/acm2/audio_coding_module.gypi',
    'neteq/neteq.gypi',
  ],
  'conditions': [
    ['include_g722==1', {
      'includes': ['codecs/g722/g722.gypi',],
    }],
    ['include_ilbc==1', {
      'includes': ['codecs/ilbc/ilbc.gypi',],
    }],
    ['include_isac==1', {
      'includes': ['codecs/isac/isac.gypi',
                   'codecs/isac/isacfix.gypi',],
    }],
    ['include_opus==1', {
      'includes': ['codecs/opus/opus.gypi',],
    }],
  ],
}
