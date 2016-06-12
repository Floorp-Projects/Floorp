#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses breakpad symbols to post-process the entries produced by
# NS_FormatCodeAddress(), which on TBPL builds often lack a file name and a
# line number (and on Linux even the symbol is often bad).

from __future__ import with_statement

import sys
import os
import re
import subprocess
import bisect

here = os.path.dirname(__file__)

def prettyFileName(name):
  if name.startswith("../") or name.startswith("..\\"):
    # dom_quickstubs.cpp and many .h files show up with relative paths that are useless
    # and/or don't correspond to the layout of the source tree.
    return os.path.basename(name) + ":"
  elif name.startswith("hg:"):
    bits = name.split(":")
    if len(bits) == 4:
      (junk, repo, path, rev) = bits
      # We could construct an hgweb URL with /file/ or /annotate/, like this:
      # return "http://%s/annotate/%s/%s#l" % (repo, rev, path)
      return path  + ":"
  return name  + ":"

class SymbolFile:
  def __init__(self, fn):
    addrs = [] # list of addresses, which will be sorted once we're done initializing
    funcs = {} # hash: address --> (function name + possible file/line)
    files = {} # hash: filenum (string) --> prettified filename ready to have a line number appended
    with open(fn) as f:
      for line in f:
        line = line.rstrip()
        # https://chromium.googlesource.com/breakpad/breakpad/+/master/docs/symbol_files.md
        if line.startswith("FUNC "):
          # FUNC <address> <size> <stack_param_size> <name>
          bits = line.split(None, 4)
          if len(bits) < 5:
            bits.append('unnamed_function')
          (junk, rva, size, ss, name) = bits
          rva = int(rva,16)
          funcs[rva] = name
          addrs.append(rva)
          lastFuncName = name
        elif line.startswith("PUBLIC "):
          # PUBLIC <address> <stack_param_size> <name>
          (junk, rva, ss, name) = line.split(None, 3)
          rva = int(rva,16)
          funcs[rva] = name
          addrs.append(rva)
        elif line.startswith("FILE "):
          # FILE <number> <name>
          (junk, filenum, name) = line.split(None, 2)
          files[filenum] = prettyFileName(name)
        elif line[0] in "0123456789abcdef":
          # This is one of the "line records" corresponding to the last FUNC record
          # <address> <size> <line> <filenum>
          (rva, size, line, filenum) = line.split(None)
          rva = int(rva,16)
          file = files[filenum]
          name = lastFuncName + " [" + file + line + "]"
          funcs[rva] = name
          addrs.append(rva)
        # skip everything else
    #print "Loaded %d functions from symbol file %s" % (len(funcs), os.path.basename(fn))
    self.addrs = sorted(addrs)
    self.funcs = funcs

  def addrToSymbol(self, address):
    i = bisect.bisect(self.addrs, address) - 1
    if i > 0:
      #offset = address - self.addrs[i]
      return self.funcs[self.addrs[i]]
    else:
      return ""

def findIdForPath(path):
  """Finds the breakpad id for the object file at the given path."""
  # We should always be packaged with a "fileid" executable.
  fileid_exe = os.path.join(here, 'fileid')
  if not os.path.isfile(fileid_exe):
    fileid_exe = fileid_exe + '.exe'
    if not os.path.isfile(fileid_exe):
      raise Exception("Could not find fileid executable in %s" % here)

  if not os.path.isfile(path):
    for suffix in ('.exe', '.dll'):
      if os.path.isfile(path + suffix):
        path = path + suffix
  try:
    return subprocess.check_output([fileid_exe, path]).rstrip()
  except subprocess.CalledProcessError as e:
    raise Exception("Error getting fileid for %s: %s" %
                    (path, e.output))

def guessSymbolFile(full_path, symbolsDir):
  """Guess a symbol file based on an object file's basename, ignoring the path and UUID."""
  fn = os.path.basename(full_path)
  d1 = os.path.join(symbolsDir, fn)
  root, _ = os.path.splitext(fn)
  if os.path.exists(os.path.join(symbolsDir, root) + '.pdb'):
    d1 = os.path.join(symbolsDir, root) + '.pdb'
    fn = root
  if not os.path.exists(d1):
    return None
  uuids = os.listdir(d1)
  if len(uuids) == 0:
    raise Exception("Missing symbol file for " + fn)
  if len(uuids) > 1:
    uuid = findIdForPath(full_path)
  else:
    uuid = uuids[0]
  return os.path.join(d1, uuid, fn + ".sym")

parsedSymbolFiles = {}
def getSymbolFile(file, symbolsDir):
  p = None
  if not file in parsedSymbolFiles:
    symfile = guessSymbolFile(file, symbolsDir)
    if symfile:
      p = SymbolFile(symfile)
    else:
      p = None
    parsedSymbolFiles[file] = p
  else:
    p = parsedSymbolFiles[file]
  return p

def addressToSymbol(file, address, symbolsDir):
  p = getSymbolFile(file, symbolsDir)
  if p:
    return p.addrToSymbol(address)
  else:
    return ""

# Matches lines produced by NS_FormatCodeAddress().
line_re = re.compile("^(.*#\d+: )(.+)\[(.+) \+(0x[0-9A-Fa-f]+)\](.*)$")

def fixSymbols(line, symbolsDir):
  result = line_re.match(line)
  if result is not None:
    (before, fn, file, address, after) = result.groups()
    address = int(address, 16)
    symbol = addressToSymbol(file, address, symbolsDir)
    if not symbol:
      symbol = "%s + 0x%x" % (os.path.basename(file), address)
    return before + symbol + after + "\n"
  else:
    return line

if __name__ == "__main__":
  symbolsDir = sys.argv[1]
  for line in iter(sys.stdin.readline, ''):
    print fixSymbols(line, symbolsDir),
