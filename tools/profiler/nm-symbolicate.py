#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys, subprocess, os

def NMSymbolicate(library, addresses):
  target_tools_prefix = os.environ.get("TARGET_TOOLS_PREFIX", "")
  args = [
    target_tools_prefix + "nm", "-D", "-S", library
  ]
  nm_lines = subprocess.check_output(args).split("\n")
  symbol_table = []
  for line in nm_lines:
    pieces = line.split(" ", 4)
    if len(pieces) != 4 or pieces[2] != "T":
      continue
    start = int(pieces[0], 16)
    end = int(pieces[1], 16)
    symbol = pieces[3]
    symbol_table.append({
      "start": int(pieces[0], 16),
      "end": int(pieces[0], 16) + int(pieces[1], 16),
      "funcName": pieces[3]
    });

  for addressStr in addresses:
    address = int(addressStr, 16)
    symbolForAddress = None
    for symbol in symbol_table:
      if address >= symbol["start"] and address <= symbol["end"]:
        symbolForAddress = symbol
        break
    if symbolForAddress:
      print symbolForAddress["funcName"]
    else:
      print "??" # match addr2line
    print ":0" # no line information from nm

if len(sys.argv) > 1:
    NMSymbolicate(sys.argv[1], sys.argv[2:])
    sys.exit(0)

print "Usage: nm-symbolicate.py <library> <addresses> > merged.sym"


