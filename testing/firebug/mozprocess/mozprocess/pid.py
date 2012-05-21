#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys

def get_pids(name, minimun_pid=0):
  """Get all the pids matching name, exclude any pids below minimum_pid."""
  # XXX see also https://bugzilla.mozilla.org/show_bug.cgi?id=592750
  
  if os.name == 'nt' or sys.platform == 'cygwin':
    import wpk
    pids = wpk.get_pids(name)
  else:
    process = subprocess.Popen(['ps', 'ax'], stdout=subprocess.PIPE)
    output, _ = process.communicate()
    data = output.splitlines()
    pids = [int(line.split()[0]) for line in data if line.find(name) is not -1]

  matching_pids = [m for m in pids if m > minimun_pid]
  return matching_pids

if __name__ == '__main__':
  import sys
  pids = set()
  for i in sys.argv[1:]:
    pids.update(get_pids(i))
  for i in sorted(pids):
    print i
