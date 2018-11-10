import subprocess
import sys

subprocess.check_call([sys.executable] + sys.argv[1:])
