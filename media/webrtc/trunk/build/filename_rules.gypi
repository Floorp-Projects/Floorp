# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This gypi file defines the patterns used for determining whether a
# file is excluded from the build on a given platform.  It is
# included by common.gypi for chromium_code.

{
  'conditions': [
    ['OS!="win"', {
      'sources/': [ ['exclude', '_win(_unittest)?\\.(h|cc)$'],
                    ['exclude', '(^|/)win/'],
                    ['exclude', '(^|/)win_[^/]*\\.(h|cc)$'] ],
    }],
    ['OS!="mac"', {
      'sources/': [ ['exclude', '_(cocoa|mac)(_unittest)?\\.(h|cc)$'],
                    ['exclude', '(^|/)(cocoa|mac)/'],
                    ['exclude', '\\.mm?$' ] ],
    }],
    # Do not exclude the linux files on *BSD since most of them can be
    # shared at this point.
    # In case a file is not needed, it is going to be excluded later on.
    # TODO(evan): the above is not correct; we shouldn't build _linux
    # files on non-linux.
    ['OS!="linux" and OS!="openbsd" and OS!="freebsd"', {
      'sources/': [
        ['exclude', '_linux(_unittest)?\\.(h|cc)$'],
        ['exclude', '(^|/)linux/'],
      ],
    }],
    ['OS!="android"', {
      'sources/': [
        ['exclude', '_android(_unittest)?\\.cc$'],
        ['exclude', '(^|/)android/'],
      ],
    }],
    ['OS=="win"', {
       'sources/': [ ['exclude', '_posix(_unittest)?\\.(h|cc)$'] ],
    }],
    ['chromeos!=1', {
      'sources/': [ ['exclude', '_chromeos\\.(h|cc)$'] ]
    }],
    ['OS!="linux" and OS!="openbsd" and OS!="freebsd"', {
      'sources/': [
        ['exclude', '_xdg(_unittest)?\\.(h|cc)$'],
      ],
    }],


    ['use_x11!=1', {
      'sources/': [
        ['exclude', '_(chromeos|x|x11)(_unittest)?\\.(h|cc)$'],
        ['exclude', '(^|/)x11_[^/]*\\.(h|cc)$'],
      ],
    }],
    ['toolkit_uses_gtk!=1', {
      'sources/': [
        ['exclude', '_gtk(_unittest)?\\.(h|cc)$'],
        ['exclude', '(^|/)gtk/'],
        ['exclude', '(^|/)gtk_[^/]*\\.(h|cc)$'],
      ],
    }],
    ['toolkit_views==0', {
      'sources/': [ ['exclude', '_views\\.(h|cc)$'] ]
    }],
    ['use_aura==0', {
      'sources/': [ ['exclude', '_aura(_unittest)?\\.(h|cc)$'],
                    ['exclude', '(^|/)aura/'],
      ]
    }],
    ['use_aura==0 or use_x11==0', {
      'sources/': [ ['exclude', '_aurax11\\.(h|cc)$'] ]
    }],
    ['use_aura==0 or OS!="win"', {
      'sources/': [ ['exclude', '_aurawin\\.(h|cc)$'] ]
    }],
    ['use_wayland!=1', {
      'sources/': [
        ['exclude', '_(wayland)(_unittest)?\\.(h|cc)$'],
        ['exclude', '(^|/)wayland/'],
        ['exclude', '(^|/)(wayland)_[^/]*\\.(h|cc)$'],
      ],
    }],
  ]
}
