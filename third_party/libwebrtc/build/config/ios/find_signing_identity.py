# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import os
import subprocess
import sys
import re


def Redact(value, from_nth_char=5):
  """Redact value past the N-th character."""
  return value[:from_nth_char] + '*' * (len(value) - from_nth_char)


class Identity(object):
  """Represents a valid identity."""

  def __init__(self, identifier, name, team):
    self.identifier = identifier
    self.name = name
    self.team = team

  def redacted(self):
    return Identity(Redact(self.identifier), self.name, Redact(self.team))

  def format(self):
    return '%s: "%s (%s)"' % (self.identifier, self.name, self.team)


def ListIdentities():
  return subprocess.check_output([
    'xcrun',
    'security',
    'find-identity',
    '-v',
    '-p',
    'codesigning',
  ])


def FindValidIdentity(pattern):
  """Find all identities matching the pattern."""
  lines = list(map(str.strip, ListIdentities().splitlines()))
  # Look for something like "2) XYZ "iPhone Developer: Name (ABC)""
  regex = re.compile('[0-9]+\) ([A-F0-9]+) "([^"(]*) \(([^)"]*)\)"')

  result = []
  for line in lines:
    res = regex.match(line)
    if res is None:
      continue
    if pattern is None or pattern in res.group(2):
      result.append(Identity(*res.groups()))
  return result


def Main(args):
  parser = argparse.ArgumentParser('codesign iOS bundles')
  parser.add_argument('--matching-pattern',
                      dest='pattern',
                      help='Pattern used to select the code signing identity.')
  parsed = parser.parse_args(args)

  identities = FindValidIdentity(parsed.pattern)
  if len(identities) == 1:
    print(identities[0].identifier, end='')
    return 0

  all_identities = FindValidIdentity(None)

  print('Automatic code signing identity selection was enabled but could not')
  print('find exactly one codesigning identity matching "%s".' % parsed.pattern)
  print('')
  print('Check that the keychain is accessible and that there is exactly one')
  print('valid codesigning identity matching the pattern. Here is the parsed')
  print('output of `xcrun security find-identity -v -p codesigning`:')
  print()
  for i, identity in enumerate(all_identities):
    print('  %d) %s' % (i + 1, identity.redacted().format()))
  print('    %d valid identities found' % (len(all_identities)))
  return 1


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
