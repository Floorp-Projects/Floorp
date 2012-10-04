#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Based on the ClusterFuzz simple fuzzer template.

import getopt
import os
import sys
import tempfile
import time

import get_user_media_fuzz


def GenerateData(input_dir):
  template = open(os.path.join(input_dir, 'template.html'))
  file_data = template.read()
  template.close()
  file_extension = 'html'

  file_data = get_user_media_fuzz.Fuzz(file_data)

  return file_data, file_extension


if __name__ == '__main__':
  start_time = time.time()

  no_of_files = None
  input_dir = None
  output_dir = None
  optlist, args = getopt.getopt(sys.argv[1:], '', \
      ['no_of_files=', 'output_dir=', 'input_dir='])
  for option, value in optlist:
    if option == '--no_of_files':     no_of_files = int(value)
    elif option == '--output_dir':    output_dir = value
    elif option == '--input_dir':     input_dir = value
  assert no_of_files is not None, 'Missing "--no_of_files" argument'
  assert output_dir is not None, 'Missing "--output_dir" argument'
  assert input_dir is not None, 'Missing "--input_dir" argument'

  for file_no in range(no_of_files):
    file_data, file_extension = GenerateData(input_dir)
    file_descriptor, file_path = tempfile.mkstemp(
        prefix='fuzz-%d-%d' % (start_time, file_no),
        suffix='.' + file_extension,
        dir=output_dir)
    file = os.fdopen(file_descriptor, 'wb')
    print 'Writing %d bytes to "%s"' % (len(file_data), file_path)
    print file_data
    file.write(file_data)
    file.close()