#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Processes an Android AAR file."""

import argparse
import os
import posixpath
import re
import shutil
import sys
from xml.etree import ElementTree
import zipfile

from util import build_utils
from util import md5_check

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                             os.pardir, os.pardir)))
import gn_helpers


# Regular expression to extract -checkdiscard / -check* lines.
# Does not support nested comments with "}" in them (oh well).
_CHECKDISCARD_PATTERN = re.compile(r'^\s*?-check.*?}\s*',
                                   re.DOTALL | re.MULTILINE)

_PROGUARD_TXT = 'proguard.txt'
_PROGUARD_CHECKS_TXT = 'proguard-checks.txt'


def _GetManifestPackage(doc):
  """Returns the package specified in the manifest.

  Args:
    doc: an XML tree parsed by ElementTree

  Returns:
    String representing the package name.
  """
  return doc.attrib['package']


def _IsManifestEmpty(doc):
  """Decides whether the given manifest has merge-worthy elements.

  E.g.: <activity>, <service>, etc.

  Args:
    doc: an XML tree parsed by ElementTree

  Returns:
    Whether the manifest has merge-worthy elements.
  """
  for node in doc:
    if node.tag == 'application':
      if node.getchildren():
        return False
    elif node.tag != 'uses-sdk':
      return False

  return True


def _CreateInfo(aar_file):
  """Extracts and return .info data from an .aar file.

  Args:
    aar_file: Path to an input .aar file.

  Returns:
    A dict containing .info data.
  """
  data = {}
  data['aidl'] = []
  data['assets'] = []
  data['resources'] = []
  data['subjars'] = []
  data['subjar_tuples'] = []
  data['has_classes_jar'] = False
  data['has_proguard_flags'] = False
  data['has_native_libraries'] = False
  data['has_r_text_file'] = False
  with zipfile.ZipFile(aar_file) as z:
    manifest_xml = ElementTree.fromstring(z.read('AndroidManifest.xml'))
    data['is_manifest_empty'] = _IsManifestEmpty(manifest_xml)
    manifest_package = _GetManifestPackage(manifest_xml)
    if manifest_package:
      data['manifest_package'] = manifest_package

    for name in z.namelist():
      if name.endswith('/'):
        continue
      if name.startswith('aidl/'):
        data['aidl'].append(name)
      elif name.startswith('res/'):
        data['resources'].append(name)
      elif name.startswith('libs/') and name.endswith('.jar'):
        label = posixpath.basename(name)[:-4]
        label = re.sub(r'[^a-zA-Z0-9._]', '_', label)
        data['subjars'].append(name)
        data['subjar_tuples'].append([label, name])
      elif name.startswith('assets/'):
        data['assets'].append(name)
      elif name.startswith('jni/'):
        data['has_native_libraries'] = True
        if 'native_libraries' in data:
          data['native_libraries'].append(name)
        else:
          data['native_libraries'] = [name]
      elif name == 'classes.jar':
        data['has_classes_jar'] = True
      elif name == _PROGUARD_TXT:
        data['has_proguard_flags'] = True
      elif name == 'R.txt':
        # Some AARs, e.g. gvr_controller_java, have empty R.txt. Such AARs
        # have no resources as well. We treat empty R.txt as having no R.txt.
        data['has_r_text_file'] = bool(z.read('R.txt').strip())

    if data['has_proguard_flags']:
      config = z.read(_PROGUARD_TXT)
      if _CHECKDISCARD_PATTERN.search(config):
        data['has_proguard_check_flags'] = True

  return data


def _SplitProguardConfig(tmp_dir):
  # Put -checkdiscard (and friends) into a separate proguard config.
  # https://crbug.com/1093831
  main_flag_path = os.path.join(tmp_dir, _PROGUARD_TXT)
  check_flag_path = os.path.join(tmp_dir, _PROGUARD_CHECKS_TXT)
  with open(main_flag_path) as f:
    config_data = f.read()
  with open(main_flag_path, 'w') as f:
    MSG = ('# Check flag moved to proguard-checks.txt by '
           '//build/android/gyp/aar.py\n')
    f.write(_CHECKDISCARD_PATTERN.sub(MSG, config_data))
  with open(check_flag_path, 'w') as f:
    f.write('# Check flags extracted by //build/android/gyp/aar.py\n\n')
    for m in _CHECKDISCARD_PATTERN.finditer(config_data):
      f.write(m.group(0))


def _PerformExtract(aar_file, output_dir, name_allowlist,
                    has_proguard_check_flags):
  with build_utils.TempDir() as tmp_dir:
    tmp_dir = os.path.join(tmp_dir, 'staging')
    os.mkdir(tmp_dir)
    build_utils.ExtractAll(
        aar_file, path=tmp_dir, predicate=name_allowlist.__contains__)
    # Write a breadcrumb so that SuperSize can attribute files back to the .aar.
    with open(os.path.join(tmp_dir, 'source.info'), 'w') as f:
      f.write('source={}\n'.format(aar_file))

    if has_proguard_check_flags:
      _SplitProguardConfig(tmp_dir)

    shutil.rmtree(output_dir, ignore_errors=True)
    shutil.move(tmp_dir, output_dir)


def _AddCommonArgs(parser):
  parser.add_argument(
      'aar_file', help='Path to the AAR file.', type=os.path.normpath)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  command_parsers = parser.add_subparsers(dest='command')
  subp = command_parsers.add_parser(
      'list', help='Output a GN scope describing the contents of the .aar.')
  _AddCommonArgs(subp)
  subp.add_argument('--output', help='Output file.', default='-')

  subp = command_parsers.add_parser('extract', help='Extracts the .aar')
  _AddCommonArgs(subp)
  subp.add_argument(
      '--output-dir',
      help='Output directory for the extracted files.',
      required=True,
      type=os.path.normpath)
  subp.add_argument(
      '--assert-info-file',
      help='Path to .info file. Asserts that it matches what '
      '"list" would output.',
      type=argparse.FileType('r'))
  subp.add_argument(
      '--ignore-resources',
      action='store_true',
      help='Whether to skip extraction of res/')

  args = parser.parse_args()

  aar_info = _CreateInfo(args.aar_file)
  formatted_info = """\
# Generated by //build/android/gyp/aar.py
# To regenerate, use "update_android_aar_prebuilts = true" and run "gn gen".

""" + gn_helpers.ToGNString(aar_info, pretty=True)

  if args.command == 'extract':
    if args.assert_info_file:
      cached_info = args.assert_info_file.read()
      if formatted_info != cached_info:
        raise Exception('android_aar_prebuilt() cached .info file is '
                        'out-of-date. Run gn gen with '
                        'update_android_aar_prebuilts=true to update it.')

    with zipfile.ZipFile(args.aar_file) as zf:
      names = zf.namelist()
      if args.ignore_resources:
        names = [n for n in names if not n.startswith('res')]

    has_proguard_check_flags = aar_info.get('has_proguard_check_flags')
    output_paths = [os.path.join(args.output_dir, n) for n in names]
    output_paths.append(os.path.join(args.output_dir, 'source.info'))
    if has_proguard_check_flags:
      output_paths.append(os.path.join(args.output_dir, _PROGUARD_CHECKS_TXT))

    def on_stale_md5():
      _PerformExtract(args.aar_file, args.output_dir, set(names),
                      has_proguard_check_flags)

    md5_check.CallAndRecordIfStale(on_stale_md5,
                                   input_strings=[aar_info],
                                   input_paths=[args.aar_file],
                                   output_paths=output_paths)

  elif args.command == 'list':
    aar_output_present = args.output != '-' and os.path.isfile(args.output)
    if aar_output_present:
      # Some .info files are read-only, for examples the cipd-controlled ones
      # under third_party/android_deps/repositoty. To deal with these, first
      # that its content is correct, and if it is, exit without touching
      # the file system.
      file_info = open(args.output, 'r').read()
      if file_info == formatted_info:
        return

    # Try to write the file. This may fail for read-only ones that were
    # not updated.
    try:
      with open(args.output, 'w') as f:
        f.write(formatted_info)
    except IOError as e:
      if not aar_output_present:
        raise e
      raise Exception('Could not update output file: %s\n%s\n' %
                      (args.output, e))

if __name__ == '__main__':
  sys.exit(main())
