#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This file emits the list of reasons why a particular build needs to be clobbered
(or a list of 'landmines').
"""

import sys

import landmine_utils


distributor = landmine_utils.distributor
gyp_defines = landmine_utils.gyp_defines
gyp_msvs_version = landmine_utils.gyp_msvs_version
platform = landmine_utils.platform


def print_landmines():
  """
  ALL LANDMINES ARE EMITTED FROM HERE.
  """
  # DO NOT add landmines as part of a regular CL. Landmines are a last-effort
  # bandaid fix if a CL that got landed has a build dependency bug and all bots
  # need to be cleaned up. If you're writing a new CL that causes build
  # dependency problems, fix the dependency problems instead of adding a
  # landmine.

  if distributor() == 'goma' and platform() == 'win32':
    print 'Need to clobber winja goma due to backend cwd cache fix.'
  if platform() == 'android':
    print 'Clobber: to handle new way of suppressing findbugs failures.'
    print 'Clobber to fix gyp not rename package name (crbug.com/457038)'
    print 'Clobber to recalculate reversed dependency (crbug.com/639042)'
  if platform() == 'win':
    print 'Compile on cc_unittests fails due to symbols removed in r185063.'
  if platform() == 'linux':
    print 'Builders switching from make to ninja will clobber on this.'
  if platform() == 'mac':
    print 'Switching from bundle to unbundled dylib (issue 14743002).'
  if platform() in ('win', 'mac'):
    print ('Improper dependency for create_nmf.py broke in r240802, '
           'fixed in r240860.')
  if platform() == 'win':
    print 'Switch to VS2015 Update 3, 14393 SDK'
  print 'Need to clobber everything due to an IDL change in r154579 (blink)'
  print 'Need to clobber everything due to gen file moves in r175513 (Blink)'
  if (platform() != 'ios'):
    print 'Clobber to get rid of obselete test plugin after r248358'
    print 'Clobber to rebuild GN files for V8'
  print 'Clobber to get rid of stale generated mojom.h files'
  print 'Need to clobber everything due to build_nexe change in nacl r13424'
  print '[chromium-dev] PSA: clobber build needed for IDR_INSPECTOR_* compil...'
  print 'blink_resources.grd changed: crbug.com/400860'
  print 'ninja dependency cycle: crbug.com/408192'
  print 'Clobber to fix missing NaCl gyp dependencies (crbug.com/427427).'
  print 'Another clobber for missing NaCl gyp deps (crbug.com/427427).'
  print 'Clobber to fix GN not picking up increased ID range (crbug.com/444902)'
  print 'Remove NaCl toolchains from the output dir (crbug.com/456902)'
  if platform() == 'ios':
    print 'Clobber iOS to workaround Xcode deps bug (crbug.com/485435)'
  if platform() == 'win':
    print 'Clobber to delete stale generated files (crbug.com/510086)'
  if platform() == 'android' and gyp_defines().get('target_arch') == 'arm64':
    print 'Clobber to support new location/infra for chrome_sync_shell_apk'
  if platform() == 'mac':
    print 'Clobber to get rid of evil libsqlite3.dylib (crbug.com/526208)'
  if platform() == 'mac':
    print 'Clobber to remove libsystem.dylib. See crbug.com/620075'
  if platform() == 'mac':
    print 'Clobber to get past mojo gen build error (crbug.com/679607)'
  if platform() == 'win':
    print 'Clobber Windows to fix strange PCH-not-rebuilt errors.'

def main():
  print_landmines()
  return 0


if __name__ == '__main__':
  sys.exit(main())
