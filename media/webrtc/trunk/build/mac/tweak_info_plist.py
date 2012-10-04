#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# Xcode supports build variable substitutions and CPP; sadly, that doesn't work
# because:
#
# 1. Xcode wants to do the Info.plist work before it runs any build phases,
#    this means if we were to generate a .h file for INFOPLIST_PREFIX_HEADER
#    we'd have to put it in another target so it runs in time.
# 2. Xcode also doesn't check to see if the header being used as a prefix for
#    the Info.plist has changed.  So even if we updated it, it's only looking
#    at the modtime of the info.plist to see if that's changed.
#
# So, we work around all of this by making a script build phase that will run
# during the app build, and simply update the info.plist in place.  This way
# by the time the app target is done, the info.plist is correct.
#

import optparse
import os
from os import environ as env
import plistlib
import re
import subprocess
import sys
import tempfile

TOP = os.path.join(env['SRCROOT'], '..')

sys.path.insert(0, os.path.join(TOP, "build/util"))
import lastchange


def _GetOutput(args):
  """Runs a subprocess and waits for termination. Returns (stdout, returncode)
  of the process. stderr is attached to the parent."""
  proc = subprocess.Popen(args, stdout=subprocess.PIPE)
  (stdout, stderr) = proc.communicate()
  return (stdout, proc.returncode)


def _GetOutputNoError(args):
  """Similar to _GetOutput() but ignores stderr. If there's an error launching
  the child (like file not found), the exception will be caught and (None, 1)
  will be returned to mimic quiet failure."""
  try:
    proc = subprocess.Popen(args, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
  except OSError:
    return (None, 1)
  (stdout, stderr) = proc.communicate()
  return (stdout, proc.returncode)


def _RemoveKeys(plist, *keys):
  """Removes a varargs of keys from the plist."""
  for key in keys:
    try:
      del plist[key]
    except KeyError:
      pass


def _AddVersionKeys(plist):
  """Adds the product version number into the plist. Returns True on success and
  False on error. The error will be printed to stderr."""
  # Pull in the Chrome version number.
  VERSION_TOOL = os.path.join(TOP, 'chrome/tools/build/version.py')
  VERSION_FILE = os.path.join(TOP, 'chrome/VERSION')

  (stdout, retval1) = _GetOutput([VERSION_TOOL, '-f', VERSION_FILE, '-t',
                                  '@MAJOR@.@MINOR@.@BUILD@.@PATCH@'])
  full_version = stdout.rstrip()

  (stdout, retval2) = _GetOutput([VERSION_TOOL, '-f', VERSION_FILE, '-t',
                                  '@BUILD@.@PATCH@'])
  bundle_version = stdout.rstrip()

  # If either of the two version commands finished with non-zero returncode,
  # report the error up.
  if retval1 or retval2:
    return False

  # Add public version info so "Get Info" works.
  plist['CFBundleShortVersionString'] = full_version

  # Honor the 429496.72.95 limit.  The maximum comes from splitting 2^32 - 1
  # into  6, 2, 2 digits.  The limitation was present in Tiger, but it could
  # have been fixed in later OS release, but hasn't been tested (it's easy
  # enough to find out with "lsregister -dump).
  # http://lists.apple.com/archives/carbon-dev/2006/Jun/msg00139.html
  # BUILD will always be an increasing value, so BUILD_PATH gives us something
  # unique that meetings what LS wants.
  plist['CFBundleVersion'] = bundle_version

  # Return with no error.
  return True


def _DoSCMKeys(plist, add_keys):
  """Adds the SCM information, visible in about:version, to property list. If
  |add_keys| is True, it will insert the keys, otherwise it will remove them."""
  scm_path, scm_revision = None, None
  if add_keys:
    version_info = lastchange.FetchVersionInfo(
        default_lastchange=None, directory=TOP)
    scm_path, scm_revision = version_info.url, version_info.revision

  # See if the operation failed.
  _RemoveKeys(plist, 'SCMRevision')
  if scm_revision != None:
    plist['SCMRevision'] = scm_revision
  elif add_keys:
    print >>sys.stderr, 'Could not determine SCM revision.  This may be OK.'

  if scm_path != None:
    plist['SCMPath'] = scm_path
  else:
    _RemoveKeys(plist, 'SCMPath')


def _DoPDFKeys(plist, add_keys):
  """Adds PDF support to the document types list. If add_keys is True, it will
  add the type information dictionary. If it is False, it will remove it if
  present."""

  PDF_FILE_EXTENSION = 'pdf'

  def __AddPDFKeys(sub_plist):
    """Writes the keys into a sub-dictionary of the plist."""
    sub_plist['CFBundleTypeExtensions'] = [PDF_FILE_EXTENSION]
    sub_plist['CFBundleTypeIconFile'] = 'document.icns'
    sub_plist['CFBundleTypeMIMETypes'] = 'application/pdf'
    sub_plist['CFBundleTypeName'] = 'PDF Document'
    sub_plist['CFBundleTypeRole'] = 'Viewer'

  DOCUMENT_TYPES_KEY = 'CFBundleDocumentTypes'

  # First get the list of document types, creating it if necessary.
  try:
    extensions = plist[DOCUMENT_TYPES_KEY]
  except KeyError:
    # If this plist doesn't have a type dictionary, create one if set to add the
    # keys. If not, bail.
    if not add_keys:
      return
    extensions = plist[DOCUMENT_TYPES_KEY] = []

  # Loop over each entry in the list, looking for one that handles PDF types.
  for i, ext in enumerate(extensions):
    # If an entry for .pdf files is found...
    if 'CFBundleTypeExtensions' not in ext:
      continue
    if PDF_FILE_EXTENSION in ext['CFBundleTypeExtensions']:
      if add_keys:
        # Overwrite the existing keys with new ones.
        __AddPDFKeys(ext)
      else:
        # Otherwise, delete the entry entirely.
        del extensions[i]
      return

  # No PDF entry exists. If one needs to be added, do so now.
  if add_keys:
    pdf_entry = {}
    __AddPDFKeys(pdf_entry)
    extensions.append(pdf_entry)


def _AddBreakpadKeys(plist, branding):
  """Adds the Breakpad keys. This must be called AFTER _AddVersionKeys() and
  also requires the |branding| argument."""
  plist['BreakpadReportInterval'] = '3600'  # Deliberately a string.
  plist['BreakpadProduct'] = '%s_Mac' % branding
  plist['BreakpadProductDisplay'] = branding
  plist['BreakpadVersion'] = plist['CFBundleShortVersionString']
  # These are both deliberately strings and not boolean.
  plist['BreakpadSendAndExit'] = 'YES'
  plist['BreakpadSkipConfirm'] = 'YES'


def _RemoveBreakpadKeys(plist):
  """Removes any set Breakpad keys."""
  _RemoveKeys(plist,
      'BreakpadURL',
      'BreakpadReportInterval',
      'BreakpadProduct',
      'BreakpadProductDisplay',
      'BreakpadVersion',
      'BreakpadSendAndExit',
      'BreakpadSkipConfirm')


def _AddKeystoneKeys(plist, bundle_identifier):
  """Adds the Keystone keys. This must be called AFTER _AddVersionKeys() and
  also requires the |bundle_identifier| argument (com.example.product)."""
  plist['KSVersion'] = plist['CFBundleShortVersionString']
  plist['KSProductID'] = bundle_identifier
  plist['KSUpdateURL'] = 'https://tools.google.com/service/update2'


def _RemoveKeystoneKeys(plist):
  """Removes any set Keystone keys."""
  _RemoveKeys(plist,
      'KSVersion',
      'KSProductID',
      'KSUpdateURL')


def Main(argv):
  parser = optparse.OptionParser('%prog [options]')
  parser.add_option('--breakpad', dest='use_breakpad', action='store',
      type='int', default=False, help='Enable Breakpad [1 or 0]')
  parser.add_option('--breakpad_uploads', dest='breakpad_uploads',
      action='store', type='int', default=False,
      help='Enable Breakpad\'s uploading of crash dumps [1 or 0]')
  parser.add_option('--keystone', dest='use_keystone', action='store',
      type='int', default=False, help='Enable Keystone [1 or 0]')
  parser.add_option('--scm', dest='add_scm_info', action='store', type='int',
      default=True, help='Add SCM metadata [1 or 0]')
  parser.add_option('--pdf', dest='add_pdf_support', action='store', type='int',
      default=False, help='Add PDF file handler support [1 or 0]')
  parser.add_option('--branding', dest='branding', action='store',
      type='string', default=None, help='The branding of the binary')
  parser.add_option('--bundle_id', dest='bundle_identifier',
      action='store', type='string', default=None,
      help='The bundle id of the binary')
  (options, args) = parser.parse_args(argv)

  if len(args) > 0:
    print >>sys.stderr, parser.get_usage()
    return 1

  # Read the plist into its parsed format.
  DEST_INFO_PLIST = os.path.join(env['TARGET_BUILD_DIR'], env['INFOPLIST_PATH'])
  plist = plistlib.readPlist(DEST_INFO_PLIST)

  # Insert the product version.
  if not _AddVersionKeys(plist):
    return 2

  # Add Breakpad if configured to do so.
  if options.use_breakpad:
    if options.branding is None:
      print >>sys.stderr, 'Use of Breakpad requires branding.'
      return 1
    _AddBreakpadKeys(plist, options.branding)
    if options.breakpad_uploads:
      plist['BreakpadURL'] = 'https://clients2.google.com/cr/report'
    else:
      # This allows crash dumping to a file without uploading the
      # dump, for testing purposes.  Breakpad does not recognise
      # "none" as a special value, but this does stop crash dump
      # uploading from happening.  We need to specify something
      # because if "BreakpadURL" is not present, Breakpad will not
      # register its crash handler and no crash dumping will occur.
      plist['BreakpadURL'] = 'none'
  else:
    _RemoveBreakpadKeys(plist)

  # Only add Keystone in Release builds.
  if options.use_keystone and env['CONFIGURATION'] == 'Release':
    if options.bundle_identifier is None:
      print >>sys.stderr, 'Use of Keystone requires the bundle id.'
      return 1
    _AddKeystoneKeys(plist, options.bundle_identifier)
  else:
    _RemoveKeystoneKeys(plist)

  # Adds or removes any SCM keys.
  _DoSCMKeys(plist, options.add_scm_info)

  # Adds or removes the PDF file handler entry.
  _DoPDFKeys(plist, options.add_pdf_support)

  # Now that all keys have been mutated, rewrite the file.
  temp_info_plist = tempfile.NamedTemporaryFile()
  plistlib.writePlist(plist, temp_info_plist.name)

  # Info.plist will work perfectly well in any plist format, but traditionally
  # applications use xml1 for this, so convert it to ensure that it's valid.
  proc = subprocess.Popen(['plutil', '-convert', 'xml1', '-o', DEST_INFO_PLIST,
                           temp_info_plist.name])
  proc.wait()
  return proc.returncode


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
