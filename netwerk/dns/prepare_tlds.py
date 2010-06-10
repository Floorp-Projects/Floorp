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
# The Original Code is Effective TLD conversion code.
#
# The Initial Developer of the Original Code is
# Jeff Walden <jwalden+code@mit.edu>.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

import codecs
import encodings.idna
import re
import sys

"""
Processes a file containing effective TLD data.  See the following URL for a
description of effective TLDs and of the file format that this script
processes (although for the latter you're better off just reading this file's
short source code).

http://wiki.mozilla.org/Gecko:Effective_TLD_Service
"""

def getEffectiveTLDs(path):
  file = codecs.open(path, "r", "UTF-8")
  domains = set()
  while True:
    line = file.readline()
    # line always contains a line terminator unless the file is empty
    if len(line) == 0:
      raise StopIteration
    line = line.rstrip()
    # comment, empty, or superfluous line for explicitness purposes
    if line.startswith("//") or "." not in line:
      continue
    line = re.split(r"[ \t\n]", line, 1)[0]
    entry = EffectiveTLDEntry(line)
    domain = entry.domain()
    assert domain not in domains, \
           "repeating domain %s makes no sense" % domain
    domains.add(domain)
    yield entry

def _normalizeHostname(domain):
  """
  Normalizes the given domain, component by component.  ASCII components are
  lowercased, while non-ASCII components are processed using the ToASCII
  algorithm.
  """
  def convertLabel(label):
    if _isASCII(label):
      return label.lower()
    return encodings.idna.ToASCII(label)
  return ".".join(map(convertLabel, domain.split(".")))

def _isASCII(s):
  "True if s consists entirely of ASCII characters, false otherwise."
  for c in s:
    if ord(c) > 127:
      return False
  return True

class EffectiveTLDEntry:
  """
  Stores an entry in an effective-TLD name file.
  """

  _exception = False
  _wild = False

  def __init__(self, line):
    """
    Creates a TLD entry from a line of data, which must have been stripped of
    the line ending.
    """
    if line.startswith("!"):
      self._exception = True
      domain = line[1:]
    elif line.startswith("*."):
      self._wild = True
      domain = line[2:]
    else:
      domain = line
    self._domain = _normalizeHostname(domain)

  def domain(self):
    "The domain this represents."
    return self._domain

  def exception(self):
    "True if this entry's domain denotes does not denote an effective TLD."
    return self._exception

  def wild(self):
    "True if this entry represents a class of effective TLDs."
    return self._wild


#################
# DO EVERYTHING #
#################

def main():
  """
  argv[1] is the effective TLD file to parse.
  A C++ array of { domain, exception, wild } entries representing the
  eTLD file is then printed to stdout.
  """

  def boolStr(b):
    if b:
      return "PR_TRUE"
    return "PR_FALSE"

  print "{"
  for etld in getEffectiveTLDs(sys.argv[1]):
    exception = boolStr(etld.exception())
    wild = boolStr(etld.wild())
    print '  { "%s", %s, %s },' % (etld.domain(), exception, wild)
  print "  { nsnull, PR_FALSE, PR_FALSE }"
  print "}"

if __name__ == '__main__':
  main()
