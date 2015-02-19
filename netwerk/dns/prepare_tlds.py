# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

def main(output, effective_tld_filename):
  """
  effective_tld_filename is the effective TLD file to parse.
  A C++ array of { domain, exception, wild } entries representing the
  eTLD file is then printed to output.
  """

  def boolStr(b):
    if b:
      return "true"
    return "false"

  for etld in getEffectiveTLDs(effective_tld_filename):
    exception = boolStr(etld.exception())
    wild = boolStr(etld.wild())
    output.write('ETLD_ENTRY("%s", %s, %s)\n' % (etld.domain(), exception, wild))
