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

import re

__statics = {}
__constructors = {}

class Parser:
  def __init__(self):
    pass
  def read(self, file):
    f = open(file)
    self.contents = f.read()
    f.close()
  def mapping(self):
    m = {}
    for p in self:
      m[p[0]] = p[1]
    return m
  def compareEntries(self, en, l10n):
    return None
  def postProcessValue(self, val):
    return val
  def __iter__(self):
    self.offset = 0
    return self
  def next(self):
    cm = self.comment.search(self.contents, self.offset)
    m  = self.key.search(self.contents, self.offset)
    if not m:
      raise StopIteration
    # eagerly strip comments
    if cm and cm.start() < m.start():
      self.offset = cm.end()
      return self.next()
    self.offset = m.end()
    return (unicode(m.group(1)), self.postProcessValue(m.group(2)))

def getParser(path):
  ext = path.rsplit('.',1)[1]
  if __statics.has_key(ext):
    return __statics[ext]
  if not __constructors.has_key(ext):
    raise UserWarning, "Cannot find Parser"
  __statics[ext] = __constructors[ext]()
  return __statics[ext]

class DTDParser(Parser):
  def __init__(self):
    self.key = re.compile('<!ENTITY\s+([\w\.]+)\s+(\"(?:[^\"]*\")|(?:\'[^\']*)\')\s*>', re.S)
    self.comment = re.compile('<!--.*?-->', re.S)
    Parser.__init__(self)

class PropertiesParser(Parser):
  def __init__(self):
    self.key = re.compile('^\s*([^#!\s\r\n][^=:\r\n]*?)\s*[:=][ \t]*(.*?)[ \t]*$',re.M)
    self.comment = re.compile('^\s*[#!].*$',re.M)
    self._post = re.compile('\\\\u([0-9a-f]+)')
    Parser.__init__(self)
  _arg_re = re.compile('%(?:(?P<cn>[0-9]+)\$)?(?P<width>[0-9]+)?(?:.(?P<pres>[0-9]+))?(?P<size>[hL]|(?:ll?))?(?P<type>[dciouxXefgpCSsn])')
  def postProcessValue(self, val):
    m = self._post.search(val)
    if not m:
      return val
    val = unicode(val)
    while m:
      uChar = unichr(int(m.group(1), 16))
      val = val.replace(m.group(), uChar)
      m = self._post.search(val)
    return val

class DefinesParser(Parser):
  def __init__(self):
    self.key = re.compile('^#define\s+(\w+)\s*(.*?)\s*$',re.M)
    self.comment = re.compile('^#[^d][^e][^f][^i][^n][^e][^\s].*$',re.M)
    Parser.__init__(self)

__constructors = {'dtd': DTDParser,
                  'properties': PropertiesParser,
                  'inc': DefinesParser}
