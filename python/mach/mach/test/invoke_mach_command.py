from __future__ import absolute_import
import subprocess
import sys

subprocess.check_call([sys.executable] + sys.argv[1:])
