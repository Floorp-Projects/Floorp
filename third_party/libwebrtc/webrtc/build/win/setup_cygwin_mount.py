# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

def main():
  if len(sys.argv) != 2 or sys.argv[1] != '--win-only':
    return 1
  if sys.platform in ('win32', 'cygwin'):
    self_dir = os.path.dirname(sys.argv[0])
    mount_path = os.path.join(self_dir, "../../third_party/cygwin")
    batch_path = os.path.join(mount_path, "setup_mount.bat")
    return os.system(os.path.normpath(batch_path) + ">nul")
  return 0


if __name__ == "__main__":
  sys.exit(main())
