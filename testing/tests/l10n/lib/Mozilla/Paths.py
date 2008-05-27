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
# Portions created by the Initial Developer are Copyright (C) 2006
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

import os.path
from subprocess import *


class Modules(dict):
  '''
  Subclass of dict to hold information on which directories belong to a
  particular app.
  It expects to have mozilla/client.mk right there from the working dir,
  and asks that for the LOCALES_foo variables.
  This only works for toolkit applications, as it's assuming that the
  apps include toolkit.
  '''
  def __init__(self, apps):
    super(dict, self).__init__()
    lapps = apps[:]
    lapps.insert(0, 'toolkit')
    of  = os.popen('make -f mozilla/client.mk ' + \
                   ' '.join(['echo-variable-LOCALES_' + app for app in lapps]))
    
    for val in of.readlines():
      self[lapps.pop(0)] = val.strip().split()
    for k,v in self.iteritems():
      if k == 'toolkit':
        continue
      self[k] = [d for d in v if d not in self['toolkit']]

class Components(dict):
  '''
  Subclass of dict to map module dirs to applications. This reverses the
  mapping you'd get from a Modules class, and it in fact uses one to do
  its job.
  '''
  def __init__(self, apps):
    modules = Modules(apps)
    for mod, lst in modules.iteritems():
      for c in lst:
        self[c] = mod

def allLocales(apps):
  '''
  Get a locales hash for the given list of applications, mapping
  applications to the list of languages given by all-locales.
  Adds a module 'toolkit' holding all languages for all applications, too.
  '''
  locales = {}
  all = set()
  for app in apps:
    path = 'mozilla/%s/locales/all-locales' % app
    locales[app] = [l.strip() for l in open(path)]
    all.update(locales[app])
  locales['toolkit'] = list(all)
  return locales

def get_base_path(mod, loc):
  'statics for path patterns and conversion'
  __l10n = 'l10n/%(loc)s/%(mod)s'
  __en_US = 'mozilla/%(mod)s/locales/en-US'
  if loc == 'en-US':
    return __en_US % {'mod': mod}
  return __l10n % {'mod': mod, 'loc': loc}

def get_path(mod, loc, leaf):
  return get_base_path(mod, loc) + '/' + leaf

