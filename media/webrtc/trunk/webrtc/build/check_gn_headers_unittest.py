#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import json
import os
import unittest
import check_gn_headers


ninja_input = r'''
obj/a.o: #deps 1, deps mtime 123 (VALID)
    ../../a.cc
    ../../dir/path/b.h
    ../../c.hh

obj/b.o: #deps 1, deps mtime 123 (STALE)
    ../../b.cc
    ../../dir2/path/b.h
    ../../c2.hh

obj/c.o: #deps 1, deps mtime 123 (VALID)
    ../../c.cc
    ../../build/a.h
    gen/b.h
    ../../dir3/path/b.h
    ../../c3.hh
'''
ninja_input_win = ninja_input.replace('/', '\\')


gn_input = json.loads(r'''
{
   "others": [],
   "targets": {
      "//:All": {
      },
      "//:base": {
         "public": [ "//base/p.h" ],
         "sources": [ "//base/a.cc", "//base/a.h", "//base/b.hh" ],
         "visibility": [ "*" ]
      },
      "//:star_public": {
         "public": "*",
         "sources": [ "//base/c.h", "//tmp/gen/a.h" ],
         "visibility": [ "*" ]
      }
    }
}
''')


whitelist = r'''
   white-front.c
a/b/c/white-end.c # comment
 dir/white-both.c  #more comment

# empty line above
a/b/c
'''


class CheckGnHeadersTest(unittest.TestCase):
  def testNinja(self):
    headers = check_gn_headers.ParseNinjaDepsOutput(ninja_input.split('\n'))
    expected = set([
        'dir/path/b.h',
        'c.hh',
        'dir3/path/b.h',
        'c3.hh',
    ])
    self.assertEquals(headers, expected)

  def testNinjaWin(self):
    old_sep = os.sep
    os.sep = '\\'

    headers = check_gn_headers.ParseNinjaDepsOutput(
        ninja_input_win.split('\n'))
    expected = set([
        'dir\\path\\b.h',
        'c.hh',
        'dir3\\path\\b.h',
        'c3.hh',
    ])
    self.assertEquals(headers, expected)

    os.sep = old_sep

  def testGn(self):
    headers = check_gn_headers.ParseGNProjectJSON(gn_input,
                                                  'out/Release', 'tmp')
    expected = set([
        'base/a.h',
        'base/b.hh',
        'base/c.h',
        'base/p.h',
        'out/Release/gen/a.h',
    ])
    self.assertEquals(headers, expected)

  def testWhitelist(self):
    output = check_gn_headers.ParseWhiteList(whitelist)
    expected = set([
        'white-front.c',
        'a/b/c/white-end.c',
        'dir/white-both.c',
        'a/b/c',
    ])
    self.assertEquals(output, expected)


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
