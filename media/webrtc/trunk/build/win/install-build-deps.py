#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shutil
import sys
import os

def patch_msbuild():
  """VS2010 MSBuild has a ULDI bug that we patch here. See http://goo.gl/Pn8tj.
  """
  source_path = os.path.join(os.environ['ProgramFiles(x86)'],
                             "MSBuild",
                             "Microsoft.Cpp",
                             "v4.0",
                             "Microsoft.CppBuild.targets")
  backup_path = source_path + ".backup"
  if not os.path.exists(backup_path):
    try:
      print "Backing up %s..." % source_path
      shutil.copyfile(source_path, backup_path)
    except IOError:
      print "Could not back up %s to %s. Run as Administrator?" % (
          source_path, backup_path)
      return 1

  source = open(source_path).read()
  base = ('''<Target Name="GetResolvedLinkObjs" Returns="@(ObjFullPath)" '''
          '''DependsOnTargets="$(CommonBuildOnlyTargets);ComputeCLOutputs;'''
          '''ResolvedLinkObjs"''')
  find = base + '>'
  replace = base + ''' Condition="'$(ConfigurationType)'=='StaticLibrary'">'''
  result = source.replace(find, replace)

  if result != source:
    open(source_path, "w").write(result)
    print "Patched %s." % source_path
  return 0


def main():
  return patch_msbuild()


if __name__ == "__main__":
  sys.exit(main())
