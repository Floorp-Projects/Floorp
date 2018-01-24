#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shim to run nacl toolchain download script only if there is a nacl dir."""

import os
import sys


def Main(args):
  # Exit early if disable_nacl=1.
  if 'disable_nacl=1' in os.environ.get('GYP_DEFINES', ''):
    return 0
  script_dir = os.path.dirname(os.path.abspath(__file__))
  src_dir = os.path.dirname(script_dir)
  nacl_dir = os.path.join(src_dir, 'native_client')
  nacl_build_dir = os.path.join(nacl_dir, 'build')
  download_script = os.path.join(nacl_build_dir, 'download_toolchains.py')
  if not os.path.exists(download_script):
    print "Can't find '%s'" % download_script
    print 'Presumably you are intentionally building without NativeClient.'
    print 'Skipping NativeClient toolchain download.'
    sys.exit(0)
  sys.path.insert(0, nacl_build_dir)
  import download_toolchains

  # TODO (robertm): Finish getting PNaCl ready for prime time.
  # BUG:
  # We remove this --optional-pnacl argument, and instead replace it with
  # --no-pnacl for most cases.  However, if the bot name is the pnacl_sdk
  # bot then we will go ahead and download it.  This prevents increasing the
  # gclient sync time for developers, or standard Chrome bots.
  if '--optional-pnacl' in args:
    args.remove('--optional-pnacl')
    # By default we don't use PNaCl toolchain yet, unless on ARM, where
    # there is no other toolchain to build untrusted code at the moment.
    # So analyze if we're building for ARM, or on SDK buildbot.
    # TODO(olonho): we need to invent more reliable way to get build
    # configuration info, to know if we're building for ARM.
    use_pnacl = False
    if 'target_arch=arm' in os.environ.get('GYP_DEFINES', ''):
      use_pnacl = True
    buildbot_name = os.environ.get('BUILDBOT_BUILDERNAME', '')
    if buildbot_name.find('pnacl') >= 0 and  buildbot_name.find('sdk') >= 0:
      use_pnacl = True
    if use_pnacl:
      print '\n*** DOWNLOADING PNACL TOOLCHAIN ***\n'
    else:
      args.append('--no-pnacl')

  # Append the name of the file to use as a version and hash source.
  # NOTE:  While not recommended, it is possible to redirect this file to
  # a chrome location to avoid branching NaCl if just a toolchain needs
  # to be bumped.
  args.append(os.path.join(nacl_dir,'TOOL_REVISIONS'))

  download_toolchains.main(args)
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
