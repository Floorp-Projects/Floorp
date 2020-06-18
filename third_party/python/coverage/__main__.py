# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

"""Be able to execute coverage.py by pointing Python at a working tree."""

import runpy
import os

PKG = 'coverage'

run_globals = runpy.run_module(PKG, run_name='__main__', alter_sys=True)
executed = os.path.splitext(os.path.basename(run_globals['__file__']))[0]
