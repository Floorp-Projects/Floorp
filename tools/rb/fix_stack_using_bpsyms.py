#!/usr/bin/env python

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is fix_stack_using_bpsyms.py.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jesse Ruderman <jruderman@gmail.com>
#   L. David Baron <dbaron@dbaron.org>
#   Ted Mielczarek <ted.mielczarek@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

from __future__ import with_statement

import sys
import os
import re
import bisect

def prettyFileName(name):
  if name.startswith("../") or name.startswith("..\\"):
    # dom_quickstubs.cpp and many .h files show up with relative paths that are useless
    # and/or don't correspond to the layout of the source tree.
    return os.path.basename(name) + ":"
  elif name.startswith("hg:"):
    (junk, repo, path, rev) = name.split(":")
    # We could construct an hgweb URL with /file/ or /annotate/, like this:
    # return "http://%s/annotate/%s/%s#l" % (repo, rev, path)
    return path  + ":"
  return name  + ":"

class readSymbolFile:
  def __init__(self, fn):
    addrs = [] # list of addresses, which will be sorted once we're done initializing
    funcs = {} # hash: address --> (function name + possible file/line)
    files = {} # hash: filenum (string) --> prettified filename ready to have a line number appended
    with open(fn) as f:
      for line in f:
        line = line.rstrip()
        # http://code.google.com/p/google-breakpad/wiki/SymbolFiles
        if line.startswith("FUNC "):
          # FUNC <address> <size> <stack_param_size> <name>
          (junk, rva, size, ss, name) = line.split(None, 4)
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


def guessSymbolFile(fn, symbolsDir):
  """Guess a symbol file based on an object file's basename, ignoring the path and UUID."""
  fn = os.path.basename(fn)
  d1 = os.path.join(symbolsDir, fn)
  if not os.path.exists(d1):
    return None
  uuids = os.listdir(d1)
  if len(uuids) == 0:
    raise Exception("Missing symbol file for " + fn)
  if len(uuids) > 1:
    raise Exception("Ambiguous symbol file for " + fn)
  return os.path.join(d1, uuids[0], fn + ".sym")

parsedSymbolFiles = {}
def addressToSymbol(file, address, symbolsDir):
  p = None
  if not file in parsedSymbolFiles:
    symfile = guessSymbolFile(file, symbolsDir)
    if symfile:
      p = readSymbolFile(symfile)
    else:
      p = None
    parsedSymbolFiles[file] = p
  else:
    p = parsedSymbolFiles[file]
  if p:
    return p.addrToSymbol(address)
  else:
    return ""

line_re = re.compile("^(.*) ?\[([^ ]*) \+(0x[0-9A-F]{1,8})\](.*)$")
balance_tree_re = re.compile("^([ \|0-9-]*)")

def fixSymbols(line, symbolsDir):
  result = line_re.match(line)
  if result is not None:
    # before allows preservation of balance trees
    # after allows preservation of counts
    (before, file, address, after) = result.groups()
    address = int(address, 16)
    # throw away the bad symbol, but keep balance tree structure
    before = balance_tree_re.match(before).groups()[0]
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
