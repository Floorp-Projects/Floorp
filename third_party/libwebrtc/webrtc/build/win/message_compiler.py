# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Runs the Microsoft Message Compiler (mc.exe). This Python adapter is for the
# GN build, which can only run Python and not native binaries.
#
# Usage: message_compiler.py <environment_file> [<args to mc.exe>*]

import os
import subprocess
import sys

def main():
  # Read the environment block from the file. This is stored in the format used
  # by CreateProcess. Drop last 2 NULs, one for list terminator, one for
  # trailing vs. separator.
  env_pairs = open(sys.argv[1]).read()[:-2].split('\0')
  env_dict = dict([item.split('=', 1) for item in env_pairs])

  # mc writes to stderr, so this explicitly redirects to stdout and eats it.
  try:
    # This needs shell=True to search the path in env_dict for the mc
    # executable.
    rest = sys.argv[2:]
    subprocess.check_output(['mc.exe'] + rest,
                            env=env_dict,
                            stderr=subprocess.STDOUT,
                            shell=True)
    # We require all source code (in particular, the header generated here) to
    # be UTF-8. jinja can output the intermediate .mc file in UTF-8 or UTF-16LE.
    # However, mc.exe only supports Unicode via the -u flag, and it assumes when
    # that is specified that the input is UTF-16LE (and errors out on UTF-8
    # files, assuming they're ANSI). Even with -u specified and UTF16-LE input,
    # it generates an ANSI header, and includes broken versions of the message
    # text in the comment before the value. To work around this, for any invalid
    # // comment lines, we simply drop the line in the header after building it.
    header_dir = None
    input_file = None
    for i, arg in enumerate(rest):
      if arg == '-h' and len(rest) > i + 1:
        assert header_dir == None
        header_dir = rest[i + 1]
      elif arg.endswith('.mc') or arg.endswith('.man'):
        assert input_file == None
        input_file = arg
    if header_dir:
      header_file = os.path.join(
          header_dir, os.path.splitext(os.path.basename(input_file))[0] + '.h')
      header_contents = []
      with open(header_file, 'rb') as f:
        for line in f.readlines():
          if line.startswith('//') and '?' in line:
            continue
          header_contents.append(line)
      with open(header_file, 'wb') as f:
        f.write(''.join(header_contents))
  except subprocess.CalledProcessError as e:
    print e.output
    sys.exit(e.returncode)

if __name__ == '__main__':
  main()
