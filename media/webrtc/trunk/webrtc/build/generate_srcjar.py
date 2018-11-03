#!/usr/bin/env python
#
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# TODO(mbonadei): move this script into chromium (build/android/gyp)
# when approved

import optparse
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.abspath(os.path.join(os.pardir, os.pardir, 'build',
                                                'android', 'gyp', 'util')))
import build_utils


def PackageToPath(src_file):
  """Returns the path of a .java file according to the package declaration.

  Example:
  src_file='/home/foo/bar/org/android/TestClass.java'
  With the following package definition:
  package org.android;

  It will return 'org/android'.

  Args:
    A string with the path of the .java source file to analyze.

  Returns:
    A string with the translation of the package definition into a path.
  """
  with open(src_file) as f:
    file_src = f.read()
    package = re.search('package (.*);', file_src).group(1)
    zip_folder = package.replace('.', os.path.sep)
    file_name = os.path.basename(src_file)
    return os.path.join(zip_folder, file_name)


def DoMain(argv):
  usage = 'usage: %prog [options] input_file(s)...'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-s', '--srcjar',
                    help='The path where the .srcjar file will be generated')

  options, args = parser.parse_args(argv)

  if not args:
    parser.error('Need to specify at least one input source file (.java)')
  input_paths = args

  with zipfile.ZipFile(options.srcjar, 'w', zipfile.ZIP_STORED) as srcjar:
    for src_path in input_paths:
      zip_path = PackageToPath(src_path)
      build_utils.AddToZipHermetic(srcjar, zip_path, src_path)


if __name__ == '__main__':
  DoMain(sys.argv[1:])
