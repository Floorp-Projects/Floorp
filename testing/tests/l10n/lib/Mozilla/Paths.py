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

modules = {'toolkit': ['netwerk','dom','toolkit','security/manager'],
           'browser': ['browser','extensions/reporter',
                       'other-licenses/branding/firefox'],
           'mail': ['mail','other-licenses/branding/thunderbird',
                    'editor/ui']}

components = {}
for mod, lst in modules.iteritems():
  for c in lst:
    components[c] = mod

locales = {}
all = {}
for app in ['browser', 'mail']:
  path = 'mozilla/%s/locales/all-locales' % app
  locales[app] = [l.strip() for l in open(path)]
  for loc in locales[app]: all[loc] = 1
all = sorted(all.keys())
locales['toolkit'] = all
#modules = {'browser': ['browser']} # XXX debug
#locales = {'browser': ['fr', 'pl']} # locales['mail']} # XXX debug
pass

def get_base_path(mod, loc):
  'statics for path patterns and conversion'
  __l10n = 'l10n/%(loc)s/%(mod)s'
  __en_US = 'mozilla/%(mod)s/locales/en-US'
  if loc == 'en-US':
    return __en_US % {'mod': mod}
  return __l10n % {'mod': mod, 'loc': loc}

def get_path(mod, loc, leaf):
  return get_base_path(mod, loc) + '/' + leaf

