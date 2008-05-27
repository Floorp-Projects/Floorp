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
import codecs
import logging

__constructors = []

class Parser:
  def __init__(self):
    if not hasattr(self, 'encoding'):
      self.encoding = 'utf-8';
    pass
  def read(self, file):
    f = codecs.open(file, 'r', self.encoding)
    try:
      self.contents = f.read()
    except UnicodeDecodeError, e:
      logging.getLogger('locales').error("Can't read file: " + file + '; ' + str(e))
      self.contents = u''
    f.close()
  def parse(self, contents):
    (self.contents, length) = codecs.getdecoder(self.encoding)(contents)
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
    return (m.group(1), self.postProcessValue(m.group(2)))

def getParser(path):
  for item in __constructors:
    if re.search(item[0], path):
      return item[1]
  raise UserWarning, "Cannot find Parser"

class DTDParser(Parser):
  def __init__(self):
    # http://www.w3.org/TR/2006/REC-xml11-20060816/#NT-NameStartChar
    #":" | [A-Z] | "_" | [a-z] |
    # [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF]
    # | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
    # [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
    # [#x10000-#xEFFFF]
    NameStartChar = u':A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF' + \
        u'\u0370-\u037D\u037F-\u1FFF\u200C-\u200D\u2070-\u218F\u2C00-\u2FEF'+\
        u'\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD'
    # + \U00010000-\U000EFFFF seems to be unsupported in python
    
    # NameChar ::= NameStartChar | "-" | "." | [0-9] | #xB7 |
    #   [#x0300-#x036F] | [#x203F-#x2040]
    NameChar = NameStartChar + ur'\-\.0-9' + u'\xB7\u0300-\u036F\u203F-\u2040'
    Name = '[' + NameStartChar + '][' + NameChar + ']*'
    self.key = re.compile('<!ENTITY\s+(' + Name + ')\s+(\"(?:[^\"]*\")|(?:\'[^\']*)\')\s*>', re.S)
    self.comment = re.compile('<!--.*?-->', re.S)
    Parser.__init__(self)

class PropertiesParser(Parser):
  def __init__(self):
    self.key = re.compile('^\s*([^#!\s\r\n][^=:\r\n]*?)\s*[:=][ \t]*(.*?)[ \t]*$',re.M)
    self.comment = re.compile('^\s*[#!].*$',re.M)
    self._post = re.compile('\\\\u([0-9a-fA-F]{4})')
    Parser.__init__(self)
  _arg_re = re.compile('%(?:(?P<cn>[0-9]+)\$)?(?P<width>[0-9]+)?(?:.(?P<pres>[0-9]+))?(?P<size>[hL]|(?:ll?))?(?P<type>[dciouxXefgpCSsn])')
  def postProcessValue(self, val):
    m = self._post.search(val)
    if not m:
      return val
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

class BookmarksParser(Parser):
  def __init__(self):
    Parser.__init__(self)
  def __iter__(self):
    def getProps(f, base):
      r = []
      if f.has_key(u'__title__'):
        r.append((base+'.title', f[u'__title__']))
      if f.has_key(u'__desc__'):
        r.append((base+'.desc',f[u'__desc__']))
      for i in range(len(f[u'children'])):
        c = f[u'children'][i]
        if c.has_key(u'children'):
          r += getProps(c, base + '.' + str(i))
        else:
          for k,v in c.iteritems():
            if k == u'HREF':
              r.append(('%s.%i.href'%(base, i), v))
            if k == u'FEEDURL':
              r.append(('%s.%i.feedurl'%(base, i), v))
            elif k == u'__title__':
              r.append(('%s.%i.title'%(base, i), v))
            elif k == u'ICON':
              r.append(('%s.%i.icon'%(base, i), v))
      return r
    return getProps(self.getDetails(), 'bookmarks').__iter__()
  def next(self):
    raise NotImplementedError
  def getDetails(self):
    def parse_link(f, line):
      link = {}
      for m in re.finditer('(?P<key>[A-Z]+)="(?P<val>[^"]+)', line):
        link[m.group('key').upper()] = m.group('val')
      m = re.search('"[ ]*>([^<]+)</A>', line, re.I)
      link[u'__title__'] = m.group(1)
      f[u'children'].append(link)
    stack = []
    f = {u'children': []}
    nextTitle = None
    nextDesc = None
    for ln in self.contents.splitlines():
      if ln.find('<DL') >= 0:
        stack.append(f)
        chld = {u'children':[], u'__title__': nextTitle, u'__desc__': nextDesc}
        nextTitle = None
        nextDesc = None
        f[u'children'].append(chld)
        f = chld
      elif re.search('<H[13]',ln):
        if nextTitle:
          pass
        nextTitle = re.search('>([^<]+)</H', ln, re.I).group(1)
      elif ln.find('<DD>') >= 0:
        if nextDesc:
          pass
        nextDesc = re.search('DD>([^<]+)', ln).group(1)
      elif ln.find('<DT><A') >= 0:
        parse_link(f, ln)
      elif ln.find('</DL>') >= 0:
        f = stack.pop()
      elif ln.find('<TITLE') >= 0:
        f[u'__title__'] = re.search('>([^<]+)</TITLE', ln).group(1)
    return f


__constructors = [('\\.dtd', DTDParser()),
                  ('\\.properties', PropertiesParser()),
                  ('\\.inc', DefinesParser()),
                  ('bookmarks\\.html', BookmarksParser())]
