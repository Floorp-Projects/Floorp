#!/usr/bin/env python
#
# This exists to paper over differences between gyp's `action` definitions
# and moz.build `GENERATED_FILES` semantics.

import buildconfig
import os
import subprocess

def main(output, *inputs):
    env=dict(os.environ)
    env['PERL'] = buildconfig.substs['PERL']
    output.write(subprocess.check_output([buildconfig.substs['PYTHON'],
                 inputs[0], inputs[2]], env=env))
    return None
