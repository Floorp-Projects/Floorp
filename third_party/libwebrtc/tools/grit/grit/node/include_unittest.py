#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for include.IncludeNode'''

from __future__ import print_function

import os
import sys
import unittest
import zlib

if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit.node import misc
from grit.node import include
from grit.node import empty
from grit import util


def checkIsGzipped(filename, compress_attr):
  test_data_root = util.PathFromRoot('grit/testdata')
  root = util.ParseGrdForUnittest(
      '''
      <includes>
        <include name="TEST_TXT" file="%s" %s type="BINDATA"/>
      </includes>''' % (filename, compress_attr),
      base_dir=test_data_root)
  node, = root.GetChildrenOfType(include.IncludeNode)
  compressed = node.GetDataPackValue(lang='en', encoding=util.BINARY)

  decompressed_data = zlib.decompress(compressed, 16 + zlib.MAX_WBITS)
  expected = util.ReadFile(os.path.join(test_data_root, filename), util.BINARY)
  return expected == decompressed_data


class IncludeNodeUnittest(unittest.TestCase):
  def testGetPath(self):
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', r'..\resource')
    release = misc.ReleaseNode()
    release.StartParsing(u'release', root)
    release.HandleAttribute(u'seq', u'1')
    root.AddChild(release)
    includes = empty.IncludesNode()
    includes.StartParsing(u'includes', release)
    release.AddChild(includes)
    include_node = include.IncludeNode()
    include_node.StartParsing(u'include', includes)
    include_node.HandleAttribute(u'file', r'flugel\kugel.pdf')
    includes.AddChild(include_node)
    root.EndParsing()

    self.assertEqual(root.ToRealPath(include_node.GetInputPath()),
                     util.normpath(
                       os.path.join(r'../resource', r'flugel/kugel.pdf')))

  def testGetPathNoBasedir(self):
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', r'..\resource')
    release = misc.ReleaseNode()
    release.StartParsing(u'release', root)
    release.HandleAttribute(u'seq', u'1')
    root.AddChild(release)
    includes = empty.IncludesNode()
    includes.StartParsing(u'includes', release)
    release.AddChild(includes)
    include_node = include.IncludeNode()
    include_node.StartParsing(u'include', includes)
    include_node.HandleAttribute(u'file', r'flugel\kugel.pdf')
    include_node.HandleAttribute(u'use_base_dir', u'false')
    includes.AddChild(include_node)
    root.EndParsing()

    last_dir = os.path.basename(os.getcwd())
    expected_path = util.normpath(os.path.join(
        u'..', last_dir, u'flugel/kugel.pdf'))
    self.assertEqual(root.ToRealPath(include_node.GetInputPath()),
                     expected_path)

  def testCompressGzip(self):
    self.assertTrue(checkIsGzipped('test_text.txt', 'compress="gzip"'))

  def testCompressGzipByDefault(self):
    self.assertTrue(checkIsGzipped('test_html.html', ''))
    self.assertTrue(checkIsGzipped('test_js.js', ''))
    self.assertTrue(checkIsGzipped('test_css.css', ''))
    self.assertTrue(checkIsGzipped('test_svg.svg', ''))

    self.assertTrue(checkIsGzipped('test_html.html', 'compress="default"'))
    self.assertTrue(checkIsGzipped('test_js.js', 'compress="default"'))
    self.assertTrue(checkIsGzipped('test_css.css', 'compress="default"'))
    self.assertTrue(checkIsGzipped('test_svg.svg', 'compress="default"'))

  def testSkipInResourceMap(self):
    root = util.ParseGrdForUnittest('''
        <includes>
          <include name="TEST1_TXT" file="test1_text.txt" type="BINDATA"/>
          <include name="TEST2_TXT" file="test1_text.txt" type="BINDATA"
                                    skip_in_resource_map="true"/>
          <include name="TEST3_TXT" file="test1_text.txt" type="BINDATA"
                                    skip_in_resource_map="false"/>
        </includes>''', base_dir = util.PathFromRoot('grit/testdata'))
    inc = root.GetChildrenOfType(include.IncludeNode)
    self.assertTrue(inc[0].IsResourceMapSource())
    self.assertFalse(inc[1].IsResourceMapSource())
    self.assertTrue(inc[2].IsResourceMapSource())

  def testAcceptsPreprocess(self):
    root = util.ParseGrdForUnittest(
        '''
        <includes>
          <include name="PREPROCESS_TEST" file="preprocess_test.html"
                   preprocess="true" compress="false" type="chrome_html"/>
        </includes>''',
        base_dir=util.PathFromRoot('grit/testdata'))
    inc, = root.GetChildrenOfType(include.IncludeNode)
    result = inc.GetDataPackValue(lang='en', encoding=util.BINARY)
    self.assertIn(b'should be kept', result)
    self.assertIn(b'in the middle...', result)
    self.assertNotIn(b'should be removed', result)


if __name__ == '__main__':
  unittest.main()
