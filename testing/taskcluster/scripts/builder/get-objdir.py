#!/usr/bin/env python2.7

from __future__ import print_function
import sys
import os
import json
import subprocess
from StringIO import StringIO

gecko_dir = sys.argv[1]
os.chdir(gecko_dir)

result = subprocess.check_output(["./mach", "environment", "--format", "json"])
environment = json.load(StringIO(result))

topobjdir = environment["mozconfig"]["topobjdir"]
if topobjdir is None:
    topobjdir = sys.argv[2]

print(topobjdir)
