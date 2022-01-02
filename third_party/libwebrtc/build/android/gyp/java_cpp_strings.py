#!/user/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys
import zipfile

from util import build_utils
from util import java_cpp_utils


class _String(object):

  def __init__(self, name, value, comments):
    self.name = java_cpp_utils.KCamelToShouty(name)
    self.value = value
    self.comments = '\n'.join('    ' + x for x in comments)

  def Format(self):
    return '%s\n    public static final String %s = %s;' % (
        self.comments, self.name, self.value)


def ParseTemplateFile(lines):
  package_re = re.compile(r'^package (.*);')
  class_re = re.compile(r'.*class (.*) {')
  package = ''
  class_name = ''
  for line in lines:
    package_line = package_re.match(line)
    if package_line:
      package = package_line.groups()[0]
    class_line = class_re.match(line)
    if class_line:
      class_name = class_line.groups()[0]
      break
  return package, class_name


# TODO(crbug.com/937282): It should be possible to parse a file for more than
# string constants. However, this currently only handles extracting string
# constants from a file (and all string constants from that file). Work will
# be needed if we want to annotate specific constants or non string constants
# in the file to be parsed.
class StringFileParser(object):
  SINGLE_LINE_COMMENT_RE = re.compile(r'\s*(// [^\n]*)')
  STRING_RE = re.compile(r'\s*const char k(.*)\[\]\s*=')
  VALUE_RE = re.compile(r'\s*("(?:\"|[^"])*")\s*;')

  def __init__(self, lines, path=''):
    self._lines = lines
    self._path = path
    self._in_string = False
    self._in_comment = False
    self._package = ''
    self._current_comments = []
    self._current_name = ''
    self._current_value = ''
    self._strings = []

  def _ExtractVariable(self, line):
    match = StringFileParser.STRING_RE.match(line)
    return match.group(1) if match else None

  def _ExtractValue(self, line):
    match = StringFileParser.VALUE_RE.search(line)
    return match.group(1) if match else None

  def _Reset(self):
    self._current_comments = []
    self._current_name = ''
    self._current_value = ''
    self._in_string = False
    self._in_comment = False

  def _AppendString(self):
    self._strings.append(
        _String(self._current_name, self._current_value,
                self._current_comments))
    self._Reset()

  def _ParseValue(self, line):
    current_value = self._ExtractValue(line)
    if current_value is not None:
      self._current_value = current_value
      self._AppendString()
    else:
      self._Reset()

  def _ParseComment(self, line):
    comment_line = StringFileParser.SINGLE_LINE_COMMENT_RE.match(line)
    if comment_line:
      self._current_comments.append(comment_line.groups()[0])
      self._in_comment = True
      self._in_string = True
      return True
    else:
      self._in_comment = False
      return False

  def _ParseString(self, line):
    current_name = self._ExtractVariable(line)
    if current_name is not None:
      self._current_name = current_name
      current_value = self._ExtractValue(line)
      if current_value is not None:
        self._current_value = current_value
        self._AppendString()
      else:
        self._in_string = True
      return True
    else:
      self._in_string = False
      return False

  def _ParseLine(self, line):
    if not self._in_string:
      if not self._ParseString(line):
        self._ParseComment(line)
      return

    if self._in_comment:
      if self._ParseComment(line):
        return
      if not self._ParseString(line):
        self._Reset()
      return

    if self._in_string:
      self._ParseValue(line)

  def Parse(self):
    for line in self._lines:
      self._ParseLine(line)
    return self._strings


def _GenerateOutput(template, source_paths, template_path, strings):
  description_template = """
    // This following string constants were inserted by
    //     {SCRIPT_NAME}
    // From
    //     {SOURCE_PATHS}
    // Into
    //     {TEMPLATE_PATH}

"""
  values = {
      'SCRIPT_NAME': java_cpp_utils.GetScriptName(),
      'SOURCE_PATHS': ',\n    //     '.join(source_paths),
      'TEMPLATE_PATH': template_path,
  }
  description = description_template.format(**values)
  native_strings = '\n\n'.join(x.Format() for x in strings)

  values = {
      'NATIVE_STRINGS': description + native_strings,
  }
  return template.format(**values)


def _ParseStringFile(path):
  with open(path) as f:
    return StringFileParser(f.readlines(), path).Parse()


def _Generate(source_paths, template_path):
  with open(template_path) as f:
    lines = f.readlines()

  template = ''.join(lines)
  package, class_name = ParseTemplateFile(lines)
  package_path = package.replace('.', os.path.sep)
  file_name = class_name + '.java'
  output_path = os.path.join(package_path, file_name)
  strings = []
  for source_path in source_paths:
    strings.extend(_ParseStringFile(source_path))

  output = _GenerateOutput(template, source_paths, template_path, strings)
  return output, output_path


def _Main(argv):
  parser = argparse.ArgumentParser()

  parser.add_argument(
      '--srcjar',
      required=True,
      help='When specified, a .srcjar at the given path is '
      'created instead of individual .java files.')

  parser.add_argument(
      '--template',
      required=True,
      help='Can be used to provide a context into which the'
      'new string constants will be inserted.')

  parser.add_argument(
      'inputs', nargs='+', help='Input file(s)', metavar='INPUTFILE')
  args = parser.parse_args(argv)

  with build_utils.AtomicOutput(args.srcjar) as f:
    with zipfile.ZipFile(f, 'w', zipfile.ZIP_STORED) as srcjar:
      data, path = _Generate(args.inputs, args.template)
      build_utils.AddToZipHermetic(srcjar, path, data=data)


if __name__ == '__main__':
  _Main(sys.argv[1:])
