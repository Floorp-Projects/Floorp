# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This source code is indirectly used in python/mozbuild/mozbuild/util.py and
# is not meant to be imported. This file can be entirely deleted when the
# transition to Python 3 is complete.

import imp
import os
import sys

orig_find_module = imp.find_module

def my_find_module(name, dirs):
    if name == main_module_name:
        path = os.path.join(dirs[0], main_file_name)
        f = open(path)
        return (f, path, ('', 'r', imp.PY_SOURCE))
    return orig_find_module(name, dirs)

# Don't allow writing bytecode file for the main module.
orig_load_module = imp.load_module

def my_load_module(name, file, path, description):
    # multiprocess.forking invokes imp.load_module manually and
    # hard-codes the name __parents_main__ as the module name.
    if name == '__parents_main__':
        old_bytecode = sys.dont_write_bytecode
        sys.dont_write_bytecode = True
        try:
            return orig_load_module(name, file, path, description)
        finally:
            sys.dont_write_bytecode = old_bytecode

    return orig_load_module(name, file, path, description)

imp.find_module = my_find_module
imp.load_module = my_load_module
from multiprocessing.forking import main

main()
