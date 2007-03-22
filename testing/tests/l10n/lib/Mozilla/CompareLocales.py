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

'Mozilla l10n compare locales tool'

import os
import re
import logging
import Parser
import Paths

def __regify(tpl):
  return tuple(map(re.compile, tpl))

exceptions = [
  # ignore langpack contributor section for mail and brownser
  __regify(('mail|browser', 'defines.inc', 'MOZ_LANGPACK_CONTRIBUTORS')),
  # ignore search engine order for browser
  __regify(('browser', 'chrome\\/browser-region\\/region\\.properties',
   'browser\\.search\\.order\.[1-9]')),
  # ignore feed engine order for browser
  __regify(('browser', 'chrome\\/browser-region\\/region\\.properties',
   'browser\\.contentHandlers\\.types\.[0-5]'))]

def __dont_ignore(tpl):
  for mod, path, key in exceptions:
    if mod.match(tpl[0]) and path.match(tpl[1]) and key.match(tpl[2]):
      return False
  return True

fl_exceptions = [
  # ignore search plugins
  __regify(('browser', 'searchplugins\\/.+\\.xml')),
  # ignore help images
  __regify(('browser', 'chrome\\/help\\/images\\/[A-Za-z-_]+\\.png'))]

def do_ignore_fl(tpl):
  for mod, path in fl_exceptions:
    if mod.match(tpl[0]) and path.match(tpl[1]):
      return True
  return False

class FileCollector:
  class Iter:
    def __init__(self, path):
      self.__base = path
    def __iter__(self):
      self.__w = os.walk(self.__base)
      try:
        self.__nextDir()
      except StopIteration:
        # empty dir, bad, but happens
        self.__i = [].__iter__()
      return self
    def __nextDir(self):
      self.__t = self.__w.next()
      cvs  = self.__t[1].index("CVS")
      del self.__t[1][cvs]
      self.__t[1].sort()
      self.__t[2].sort()
      self.__i = self.__t[2].__iter__()
    def next(self):
      try:
        leaf = self.__i.next()
        path = self.__t[0] + '/' + leaf
        key = path[len(self.__base) + 1:]
        return (key, path)
      except StopIteration:
        self.__nextDir()
        return self.next()
      print "not expected"
      raise StopIteration
  def __init__(self):
    pass
  def getFiles(self, mod, locale):
    fls = {}
    for leaf, path in self.iterateFiles(mod,locale):
      fls[leaf] = path
    return fls
  def iterateFiles(self, mod, locale):
    return FileCollector.Iter(Paths.get_base_path(mod, locale))

def collectFiles(aComparer):
  'returns new files, files to compare, files to remove'
  en = FileCollector()
  l10n = FileCollector()
  for cat in Paths.locales.keys():
    logging.debug(" testing " + cat+ " on " + str(Paths.modules))
    aComparer.notifyLocales(cat, Paths.locales[cat])
    for mod in Paths.modules[cat]:
      en_fls = en.getFiles(mod, 'en-US')
      for loc in Paths.locales[cat]:
        fls = dict(en_fls) # create copy for modification
        for l_fl, l_path in l10n.iterateFiles(mod, loc):
          if fls.has_key(l_fl):
            # file in both en-US and locale, compare
            aComparer.compareFile(mod, loc, l_fl)
            del fls[l_fl]
          else:
            # file in locale, but not in en-US, remove?
            aComparer.removeFile(mod, loc, l_fl)
        # all locale files dealt with, remaining fls need to be added?
        for lf in fls.keys():
          aComparer.addFile(mod,loc,lf)

class CompareCollector:
  'collects files to be compared, added, removed'
  def __init__(self):
    self.cl = {}
    self.files = {}
    self.modules = {}
  def notifyLocales(self, aModule, aLocaleList):
    for loc in aLocaleList:
      if self.modules.has_key(loc):
        self.modules[loc].append(aModule)
      else:
        self.modules[loc] = [aModule]
  def addFile(self, aModule, aLocale, aLeaf):
    if do_ignore_fl((aModule, aLeaf)):
      logging.debug(" ignoring %s from %s in %s" % (aLeaf, aLocale, aModule))
      return
    logging.debug(" add %s for %s in %s" % (aLeaf, aLocale, aModule))
    if not self.files.has_key(aLocale):
      self.files[aLocale] = {'missingFiles': [(aModule, aLeaf)],
                             'obsoleteFiles': []}
    else:
      self.files[aLocale]['missingFiles'].append((aModule, aLeaf))
    pass
  def compareFile(self, aModule, aLocale, aLeaf):
    if not self.cl.has_key((aModule, aLeaf)):
      self.cl[(aModule, aLeaf)] = [aLocale]
    else:
      self.cl[(aModule, aLeaf)].append(aLocale)
    pass
  def removeFile(self, aModule, aLocale, aLeaf):
    if do_ignore_fl((aModule, aLeaf)):
      logging.debug(" ignoring %s from %s in %s" % (aLeaf, aLocale, aModule))
      return
    logging.debug(" remove %s from %s in %s" % (aLeaf, aLocale, aModule))
    if not self.files.has_key(aLocale):
      self.files[aLocale] = {'obsoleteFiles': [(aModule, aLeaf)],
                             'missingFiles':[]}
    else:
      self.files[aLocale]['obsoleteFiles'].append((aModule, aLeaf))
    pass

def compare(testLocales=[]):
  result = {}
  c = CompareCollector()
  collectFiles(c)
  for fl, locales in c.cl.iteritems():
    (mod,path) = fl
    try:
      parser = Parser.getParser(path)
    except UserWarning:
      logging.warning(" Can't compare " + path + " in " + mod)
      continue
    parser.read(Paths.get_path(mod, 'en-US', path))
    enMap = parser.mapping()
    for loc in locales:
      if not result.has_key(loc):
        result[loc] = {'missing':[],'changed':0,'unchanged':0,'obsolete':[]}
      enTmp = dict(enMap)
      parser.read(Paths.get_path(mod, loc, path))
      for k,v in parser:
        if not __dont_ignore((mod, path, k)):
          if enTmp.has_key(k):
            del enTmp[k]
          continue
        if not enTmp.has_key(k):
          result[loc]['obsolete'].append((mod,path,k))
          continue
        enVal = enTmp[k]
        del enTmp[k]
        if enVal == v:
          result[loc]['unchanged'] +=1
        else:
          result[loc]['changed'] +=1
      result[loc]['missing'].extend(filter(__dont_ignore, [(mod,path,k) for k in enTmp.keys()]))
  for loc,dics in c.files.iteritems():
    if not result.has_key(loc):
      result[loc] = dics
    else:
      for key, list in dics.iteritems():
        result[loc][key] = list
  for loc, mods in c.modules.iteritems():
    result[loc]['tested'] = mods
  return result
