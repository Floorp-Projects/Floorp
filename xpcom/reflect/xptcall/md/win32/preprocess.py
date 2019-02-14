# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import shlex
import subprocess
import tempfile
import which
import buildconfig


def preprocess(out, asm_file):
    cxx = shlex.split(buildconfig.substs['CXX'])
    if not os.path.exists(cxx[0]):
        cxx[0] = which.which(cxx[0])
    cppflags = buildconfig.substs['OS_CPPFLAGS']

    # subprocess.Popen(stdout=) only accepts actual file objects, which `out`,
    # above, is not.  So fake a temporary file to write to.
    (outhandle, tmpout) = tempfile.mkstemp(suffix='.cpp')

    # #line directives will confuse armasm64, and /EP is the only way to
    # preprocess without emitting #line directives.
    command = cxx + ['/EP'] + cppflags + ['/TP', asm_file]
    with open(tmpout, 'wb') as t:
        result = subprocess.Popen(command, stdout=t).wait()
        if result != 0:
            sys.exit(result)

    with open(tmpout, 'rb') as t:
        out.write(t.read())

    os.close(outhandle)
    os.remove(tmpout)
