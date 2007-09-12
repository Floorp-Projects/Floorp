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
# The Original Code is standalone Firefox Windows performance test.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Annie Sullivan <annie.sullivan@gmail.com> (original author)
#   Ben Hearsum    <bhearsum@wittydomain.com> (OS independence)
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


import win32pdh
import win32pdhutil


class CounterManager:

  def __init__(self, process, counters=None):
    self.process = process
    self.registeredCounters = {}
    self.registerCounters(counters)
    # PDH might need to be "refreshed" if it has been queried while Firefox
    # is closed
    win32pdh.EnumObjects(None, None, 0, 1)

  def registerCounters(self, counters):
    for counter in counters:
      self.registeredCounters[counter] = []
            
  def unregisterCounters(self, counters):
    for counter in counters:
      if counter in self.registeredCounters:
        del self.registeredCounters[counter]

  def getRegisteredCounters(self):
    return keys(self.registeredCounters)

  def getCounterValue(self, counter):
    hq = self.registeredCounters[counter][0]
    hc = self.registeredCounters[counter][1]
    try:
      win32pdh.CollectQueryData(hq)
      type, val = win32pdh.GetFormattedCounterValue(hc, win32pdh.PDH_FMT_LONG)
      return val
    except:
      return None

  def getProcess(self):
    return self.process

  def startMonitor(self):
    # PDH might need to be "refreshed" if it has been queried while Firefox
    # is closed
    win32pdh.EnumObjects(None, None, 0, 1)

    for counter in self.registeredCounters:
      path = win32pdh.MakeCounterPath((None, 'process', self.process,
                                       None, -1, counter))
      hq = win32pdh.OpenQuery()
      try:
        hc = win32pdh.AddCounter(hq, path)
      except:
        win32pdh.CloseQuery(hq)

      self.registeredCounters[counter] = [hq, hc]

  def stopMonitor(self):
    for counter in self.registeredCounters:
      win32pdh.RemoveCounter(self.registeredCounters[counter][1])
      win32pdh.CloseQuery(self.registeredCounters[counter][0])
    self.registeredCounters.clear()
