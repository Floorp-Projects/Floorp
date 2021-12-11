# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import plistlib
import os
import re
import subprocess
import sys
import tempfile
import shlex


# Xcode substitutes variables like ${PRODUCT_NAME} when compiling Info.plist.
# It also supports supports modifiers like :identifier or :rfc1034identifier.
# SUBST_RE matches a variable substitution pattern with an optional modifier,
# while IDENT_RE matches all characters that are not valid in an "identifier"
# value (used when applying the modifier).
SUBST_RE = re.compile(r'\$\{(?P<id>[^}]*?)(?P<modifier>:[^}]*)?\}')
IDENT_RE = re.compile(r'[_/\s]')


def InterpolateList(values, substitutions):
  """Interpolates variable references into |value| using |substitutions|.

  Inputs:
    values: a list of values
    substitutions: a mapping of variable names to values

  Returns:
    A new list of values with all variables references ${VARIABLE} replaced
    by their value in |substitutions| or None if any of the variable has no
    subsitution.
  """
  result = []
  for value in values:
    interpolated = InterpolateValue(value, substitutions)
    if interpolated is None:
      return None
    result.append(interpolated)
  return result


def InterpolateString(value, substitutions):
  """Interpolates variable references into |value| using |substitutions|.

  Inputs:
    value: a string
    substitutions: a mapping of variable names to values

  Returns:
    A new string with all variables references ${VARIABLES} replaced by their
    value in |substitutions| or None if any of the variable has no substitution.
  """
  result = value
  for match in reversed(list(SUBST_RE.finditer(value))):
    variable = match.group('id')
    if variable not in substitutions:
      return None
    # Some values need to be identifier and thus the variables references may
    # contains :modifier attributes to indicate how they should be converted
    # to identifiers ("identifier" replaces all invalid characters by '_' and
    # "rfc1034identifier" replaces them by "-" to make valid URI too).
    modifier = match.group('modifier')
    if modifier == ':identifier':
      interpolated = IDENT_RE.sub('_', substitutions[variable])
    elif modifier == ':rfc1034identifier':
      interpolated = IDENT_RE.sub('-', substitutions[variable])
    else:
      interpolated = substitutions[variable]
    result = result[:match.start()] + interpolated + result[match.end():]
  return result


def InterpolateValue(value, substitutions):
  """Interpolates variable references into |value| using |substitutions|.

  Inputs:
    value: a value, can be a dictionary, list, string or other
    substitutions: a mapping of variable names to values

  Returns:
    A new value with all variables references ${VARIABLES} replaced by their
    value in |substitutions| or None if any of the variable has no substitution.
  """
  if isinstance(value, dict):
    return Interpolate(value, substitutions)
  if isinstance(value, list):
    return InterpolateList(value, substitutions)
  if isinstance(value, str):
    return InterpolateString(value, substitutions)
  return value


def Interpolate(plist, substitutions):
  """Interpolates variable references into |value| using |substitutions|.

  Inputs:
    plist: a dictionary representing a Property List (.plist) file
    substitutions: a mapping of variable names to values

  Returns:
    A new plist with all variables references ${VARIABLES} replaced by their
    value in |substitutions|. All values that contains references with no
    substitutions will be removed and the corresponding key will be cleared
    from the plist (not recursively).
  """
  result = {}
  for key in plist:
    value = InterpolateValue(plist[key], substitutions)
    if value is not None:
      result[key] = value
  return result


def LoadPList(path):
  """Loads Plist at |path| and returns it as a dictionary."""
  fd, name = tempfile.mkstemp()
  try:
    subprocess.check_call(['plutil', '-convert', 'xml1', '-o', name, path])
    with os.fdopen(fd, 'r') as f:
      return plistlib.readPlist(f)
  finally:
    os.unlink(name)


def SavePList(path, format, data):
  """Saves |data| as a Plist to |path| in the specified |format|."""
  fd, name = tempfile.mkstemp()
  try:
    with os.fdopen(fd, 'w') as f:
      plistlib.writePlist(data, f)
    subprocess.check_call(['plutil', '-convert', format, '-o', path, name])
  finally:
    os.unlink(name)


def MergePList(plist1, plist2):
  """Merges |plist1| with |plist2| recursively.

  Creates a new dictionary representing a Property List (.plist) files by
  merging the two dictionary |plist1| and |plist2| recursively (only for
  dictionary values). List value will be concatenated.

  Args:
    plist1: a dictionary representing a Property List (.plist) file
    plist2: a dictionary representing a Property List (.plist) file

  Returns:
    A new dictionary representing a Property List (.plist) file by merging
    |plist1| with |plist2|. If any value is a dictionary, they are merged
    recursively, otherwise |plist2| value is used. If values are list, they
    are concatenated.
  """
  if not isinstance(plist1, dict) or not isinstance(plist2, dict):
    if plist2 is not None:
      return plist2
    else:
      return plist1
  result = {}
  for key in set(plist1) | set(plist2):
    if key in plist2:
      value = plist2[key]
    else:
      value = plist1[key]
    if isinstance(value, dict):
      value = MergePList(plist1.get(key, None), plist2.get(key, None))
    if isinstance(value, list):
      value = plist1.get(key, []) + plist2.get(key, [])
    result[key] = value
  return result


class Action(object):
  """Class implementing one action supported by the script."""

  @classmethod
  def Register(cls, subparsers):
    parser = subparsers.add_parser(cls.name, help=cls.help)
    parser.set_defaults(func=cls._Execute)
    cls._Register(parser)


class MergeAction(Action):
  """Class to merge multiple plist files."""

  name = 'merge'
  help = 'merge multiple plist files'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '-o', '--output', required=True,
        help='path to the output plist file')
    parser.add_argument(
        '-f', '--format', required=True, choices=('xml1', 'binary1', 'json'),
        help='format of the plist file to generate')
    parser.add_argument(
          'path', nargs="+",
          help='path to plist files to merge')

  @staticmethod
  def _Execute(args):
    data = {}
    for filename in args.path:
      data = MergePList(data, LoadPList(filename))
    SavePList(args.output, args.format, data)


class SubstituteAction(Action):
  """Class implementing the variable substitution in a plist file."""

  name = 'substitute'
  help = 'perform pattern substitution in a plist file'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '-o', '--output', required=True,
        help='path to the output plist file')
    parser.add_argument(
        '-t', '--template', required=True,
        help='path to the template file')
    parser.add_argument(
        '-s', '--substitution', action='append', default=[],
        help='substitution rule in the format key=value')
    parser.add_argument(
        '-f', '--format', required=True, choices=('xml1', 'binary1', 'json'),
        help='format of the plist file to generate')

  @staticmethod
  def _Execute(args):
    substitutions = {}
    for substitution in args.substitution:
      key, value = substitution.split('=', 1)
      substitutions[key] = value
    data = Interpolate(LoadPList(args.template), substitutions)
    SavePList(args.output, args.format, data)


def Main():
  parser = argparse.ArgumentParser(description='manipulate plist files')
  subparsers = parser.add_subparsers()

  for action in [MergeAction, SubstituteAction]:
    action.Register(subparsers)

  args = parser.parse_args()
  args.func(args)


if __name__ == '__main__':
  sys.exit(Main())
