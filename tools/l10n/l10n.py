#!/bin/env python

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
# The Original Code is l10n test automation.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#	Axel Hecht <l10n@mozilla.com>
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

import logging
from optparse import OptionParser
import os
import os.path
import re
from subprocess import Popen, PIPE
from shutil import copy2
import sys

def walk(base):
  for root, dirs, files in os.walk(base):
    try:
      dirs.remove('CVS')
    except ValueError:
      pass
    yield (os.path.normpath(root[len(base)+1:]), files)

def createLocalization(source, dest, apps, exceptions = {}):
  '''
  Creates a new localization.

  @type  source: string
  @param source: path to the mozilla sources to use

  @type  dest: string
  @param dest: path to the localization to create or update

  @type  apps: array of strings
  @param apps: the applications for which to create or update the
               localization

  @type  exceptions: mapping
  @param exceptions: stuff to ignore
  '''

  assert os.path.isdir(source), "source directory is not a directory"
  clientmk = os.path.join(source,'client.mk')
  assert os.path.isfile(clientmk), "client.mk missing"

  if not apps or not len(apps):
    apps=['browser']

  if not os.path.isdir(dest):
    os.makedirs(dest)

  assert os.path.isdir(dest), "target should be a directory"

  # get the directories to iterate over
  dirs = set()
  cmd = ['make', '-f', clientmk] + \
        ['echo-variable-LOCALES_' + app for app in apps]
  p = Popen(cmd, stdout = PIPE)
  for ln in p.stdout.readlines():
    dirs.update(ln.strip().split())
  dirs = sorted(list(dirs))

  for d in dirs:
    assert os.path.isdir(os.path.join(source, d)), \
           "expecting source directory %s" % d

  for d in dirs:
    logging.debug('processing %s' % d)
    if d in exceptions and exceptions[d] == 'all':
      continue

    basepath = os.path.join(source, d, 'locales', 'en-US')
    
    ign_mod = {}
    if d in exceptions:
      ign_mod = exceptions[d]
      logging.debug('using exceptions: %s' % str(ign_mod))

    l10nbase = os.path.join(dest, d)
    if not os.path.isdir(l10nbase):
      os.makedirs(l10nbase)
    
    for root, files in walk(basepath):
      ignore = None
      if root in ign_mod:
        if ign_mod[root] == '.':
          continue
        ignore = re.compile(ign_mod[root])
      l10npath = os.path.join(l10nbase, root)
      if not os.path.isdir(l10npath):
        os.mkdir(l10npath)

      for f in files:
        if ignore and ignore.search(f):
          # ignoring some files
          continue
        if not os.path.exists(os.path.join(l10npath,f)):
          copy2(os.path.join(basepath, root, f), l10npath)

if __name__ == '__main__':
  p = OptionParser()
  p.add_option('--source', default = '.',
               help='Mozilla sources')
  p.add_option('--dest', default = '../l10n',
               help='Localization target directory')
  p.add_option('--app', action="append",
               help='Create localization for this application ' + \
               '(multiple applications allowed, default: browser)')
  p.add_option('-v', '--verbose', action="store_true", default=False,
               help='report debugging information')
  (opts, args) = p.parse_args()
  if opts.verbose:
    logging.basicConfig(level=logging.DEBUG)
  assert len(args) == 1, "language code expected"
  
  # hardcoding exceptions, work for both trunk and 1.8 branch
  exceptions = {'browser':
                {'searchplugins': '\\.xml$'},
                'extensions/spellcheck': 'all'}
  createLocalization(opts.source, os.path.join(opts.dest, args[0]),
                     opts.app, exceptions=exceptions)
