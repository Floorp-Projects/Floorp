#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This exists to paper over differences between gyp's `action` definitions
# and moz.build `GENERATED_FILES` semantics.

import buildconfig
import os
import subprocess


def main(output, *inputs):
    env = dict(os.environ)
    env['PERL'] = str(buildconfig.substs['PERL'])
    output.write(subprocess.check_output([buildconfig.substs['PYTHON3'],
                                          inputs[0], inputs[2]], env=env))
    return None
