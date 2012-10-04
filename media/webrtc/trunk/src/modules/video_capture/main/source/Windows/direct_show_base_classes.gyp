# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This target is broken out into its own gyp file in order to be treated as
# third party code. (Since src/build/common.gypi is not included,
# chromium_code is disabled).
#
# We can't place this in third_party/ because Chromium parses
# video_capture.gypi and would fail to find it in the Chromium third_party/.
{
  'targets': [
    {
      'target_name': 'direct_show_base_classes',
      'type': 'static_library',
      'variables': {
        # Path needed to build the Direct Show base classes on Windows. The
        # code is included in the Windows SDK.
        'direct_show_dir%':
          'C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses/',
      },
      'defines!': [
        'NOMINMAX',
      ],
      'include_dirs': [
        '<(direct_show_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(direct_show_dir)',
        ],
      },
      'sources': [
        '<(direct_show_dir)amextra.cpp',
        '<(direct_show_dir)amextra.h',
        '<(direct_show_dir)amfilter.cpp',
        '<(direct_show_dir)amfilter.h',
        '<(direct_show_dir)amvideo.cpp',
        '<(direct_show_dir)cache.h',
        '<(direct_show_dir)combase.cpp',
        '<(direct_show_dir)combase.h',
        '<(direct_show_dir)cprop.cpp',
        '<(direct_show_dir)cprop.h',
        '<(direct_show_dir)ctlutil.cpp',
        '<(direct_show_dir)ctlutil.h',
        '<(direct_show_dir)ddmm.cpp',
        '<(direct_show_dir)ddmm.h',
        '<(direct_show_dir)dllentry.cpp',
        '<(direct_show_dir)dllsetup.cpp',
        '<(direct_show_dir)dllsetup.h',
        '<(direct_show_dir)fourcc.h',
        '<(direct_show_dir)measure.h',
        '<(direct_show_dir)msgthrd.h',
        '<(direct_show_dir)mtype.cpp',
        '<(direct_show_dir)mtype.h',
        '<(direct_show_dir)outputq.cpp',
        '<(direct_show_dir)outputq.h',
        '<(direct_show_dir)pstream.cpp',
        '<(direct_show_dir)pstream.h',
        '<(direct_show_dir)pullpin.cpp',
        '<(direct_show_dir)pullpin.h',
        '<(direct_show_dir)refclock.cpp',
        '<(direct_show_dir)refclock.h',
        '<(direct_show_dir)reftime.h',
        '<(direct_show_dir)renbase.cpp',
        '<(direct_show_dir)renbase.h',
        '<(direct_show_dir)schedule.cpp',
        '<(direct_show_dir)seekpt.cpp',
        '<(direct_show_dir)seekpt.h',
        '<(direct_show_dir)source.cpp',
        '<(direct_show_dir)source.h',
        '<(direct_show_dir)streams.h',
        '<(direct_show_dir)strmctl.cpp',
        '<(direct_show_dir)strmctl.h',
        '<(direct_show_dir)sysclock.cpp',
        '<(direct_show_dir)sysclock.h',
        '<(direct_show_dir)transfrm.cpp',
        '<(direct_show_dir)transfrm.h',
        '<(direct_show_dir)transip.cpp',
        '<(direct_show_dir)transip.h',
        '<(direct_show_dir)videoctl.cpp',
        '<(direct_show_dir)videoctl.h',
        '<(direct_show_dir)vtrans.cpp',
        '<(direct_show_dir)vtrans.h',
        '<(direct_show_dir)winctrl.cpp',
        '<(direct_show_dir)winctrl.h',
        '<(direct_show_dir)winutil.cpp',
        '<(direct_show_dir)winutil.h',
        '<(direct_show_dir)wxdebug.cpp',
        '<(direct_show_dir)wxdebug.h',
        '<(direct_show_dir)wxlist.cpp',
        '<(direct_show_dir)wxlist.h',
        '<(direct_show_dir)wxutil.cpp',
        '<(direct_show_dir)wxutil.h',
      ],
    },
  ],
}
