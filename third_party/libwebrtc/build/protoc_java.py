#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate java source files from protobuf files.

This is a helper file for the genproto_java action in protoc_java.gypi.

It performs the following steps:
1. Deletes all old sources (ensures deleted classes are not part of new jars).
2. Creates source directory.
3. Generates Java files using protoc (output into either --java-out-dir or
   --srcjar).
4. Creates a new stamp file.
"""

from __future__ import print_function

import os
import optparse
import shutil
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "android", "gyp"))
from util import build_utils

def main(argv):
  parser = optparse.OptionParser()
  build_utils.AddDepfileOption(parser)
  parser.add_option("--protoc", help="Path to protoc binary.")
  parser.add_option("--proto-path", help="Path to proto directory.")
  parser.add_option("--java-out-dir",
      help="Path to output directory for java files.")
  parser.add_option("--srcjar", help="Path to output srcjar.")
  parser.add_option("--stamp", help="File to touch on success.")
  parser.add_option("--nano",
      help="Use to generate nano protos.", action='store_true')
  parser.add_option("--import-dir", action="append", default=[],
                    help="Extra import directory for protos, can be repeated.")
  options, args = parser.parse_args(argv)

  build_utils.CheckOptions(options, parser, ['protoc', 'proto_path'])
  if not options.java_out_dir and not options.srcjar:
    print('One of --java-out-dir or --srcjar must be specified.')
    return 1

  proto_path_args = ['--proto_path', options.proto_path]
  for path in options.import_dir:
    proto_path_args += ["--proto_path", path]

  with build_utils.TempDir() as temp_dir:
    if options.nano:
      # Specify arguments to the generator.
      generator_args = ['optional_field_style=reftypes',
                        'store_unknown_fields=true']
      out_arg = '--javanano_out=' + ','.join(generator_args) + ':' + temp_dir
    else:
      out_arg = '--java_out=lite:' + temp_dir

    # Generate Java files using protoc.
    build_utils.CheckOutput(
        [options.protoc] + proto_path_args + [out_arg] + args,
        # protoc generates superfluous warnings about LITE_RUNTIME deprecation
        # even though we are using the new non-deprecated method.
        stderr_filter=lambda output: build_utils.FilterLines(
            output, '|'.join([r'optimize_for = LITE_RUNTIME', r'java/lite\.md'])
        ))

    if options.java_out_dir:
      build_utils.DeleteDirectory(options.java_out_dir)
      shutil.copytree(temp_dir, options.java_out_dir)
    else:
      build_utils.ZipDir(options.srcjar, temp_dir)

  if options.depfile:
    assert options.srcjar
    deps = args + [options.protoc]
    build_utils.WriteDepfile(options.depfile, options.srcjar, deps)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
