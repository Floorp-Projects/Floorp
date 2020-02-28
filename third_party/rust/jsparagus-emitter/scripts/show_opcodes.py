#!/usr/bin/env python3

""" Extract opcodes from C++ header.

To use, pipe the output of this command to your clipboard,
then paste it into opcode.rs.
"""

import sys

if sys.version < (3,):
    raise Exception("Please run this in Python 3.")

with open("../../../../src/vm/Opcodes.h") as f:
    for line in f:
        line = line.strip()
        if line.startswith('MACRO(') and ',' in line:
            line = line[5:]
            if line.endswith(' \\'):
                line = line[:-2]
            assert line.endswith(')')
            print((" " * 16) + line + ",")

