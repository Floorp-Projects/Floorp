#!/usr/bin/env python
#
# This exists to paper over differences between gyp's `action` definitions
# and moz.build `GENERATED_FILES` semantics.

import buildconfig
import subprocess

def main(output, *inputs):
    output.write(subprocess.check_output([buildconfig.substs['PERL']] + list(inputs)))
    return None
