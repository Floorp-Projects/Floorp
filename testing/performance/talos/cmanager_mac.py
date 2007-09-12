#!/usr/bin/env python
#
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
#   Ben Hearsum    <bhearsum@wittydomain.com> (ported to linux)
#   Zach Lipton    <zach@zachlipton.com>  (Mac port)
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

"""A set of functions to run the Tp test.

   The Tp test measures page load times in Firefox.  It does this with a
   JavaScript script that opens a new window and cycles through page loads
   from the local disk, timing each one.  The script dumps the sum of the
   mean times to open each page, and the standard deviation, to standard out.
   We can also measure performance attributes during the test.  See below for
   what can be monitored
"""

__author__ = 'annie.sullivan@gmail.com (Annie Sullivan)'


import os
import time
import threading
import subprocess

import ffprocess

def GetProcessData(pid):
  """Runs a ps on the process identified by pid and returns the output line
    as a list (uid, pid, ppid, cpu, pri, ni, vsz, rss, wchan, stat, tt, time, command)
  """
  command = ['ps -Acup'+str(pid)]
  handle = subprocess.Popen(command, stdout=subprocess.PIPE, universal_newlines=True, shell=True)
  handle.wait()
  data = handle.stdout.readlines()
  
  # find all matching processes and add them to the list
  for line in data:
    if line.find(str(pid)) >= 0:
      # splits by whitespace
      line = line.split()
      if (line[1] == str(pid)):
        return line

def GetPrivateBytes(pid):
  """Calculate the amount of private, writeable memory allocated to a process.
  """
  psData = GetProcessData(pid)
  return psData[5]


def GetResidentSize(pid):
  """Retrieve the current resident memory for a given process"""
  psData = GetProcessData(pid)
  return psData[4]

def GetCpuTime(pid):
  # return all zeros for now on this platform as per 7/18/07 perf meeting
  return 0

counterDict = {}
counterDict["Private Bytes"] = GetPrivateBytes
counterDict["RSS"] = GetResidentSize
counterDict["% Processor Time"] = GetCpuTime

class CounterManager(threading.Thread):
  """This class manages the monitoring of a process with any number of
     counters.

     A counter can be any function that takes an argument of one pid and
     returns a piece of data about that process.
     Some examples are: CalcCPUTime, GetResidentSize, and GetPrivateBytes
  """
  
  pollInterval = .25

  def __init__(self, process, counters=None):
    """Args:
         counters: A list of counters to monitor. Any counters whose name does
         not match a key in 'counterDict' will be ignored.
    """
    self.allCounters = {}
    self.registeredCounters = {}
    self.process = process
    self.runThread = False
    self.pid = -1

    self._loadCounters()
    self.registerCounters(counters)

    threading.Thread.__init__(self)

  def _loadCounters(self):
    """Loads all of the counters defined in the counterDict"""
    for counter in counterDict.keys():
      self.allCounters[counter] = counterDict[counter]

  def registerCounters(self, counters):
    """Registers a list of counters that will be monitoring.
       Only counters whose names are found in allCounters will be added
    """
    for counter in counters:
      if counter in self.allCounters:
        self.registeredCounters[counter] = \
          [self.allCounters[counter], []]

  def unregisterCounters(self, counters):
    """Unregister a list of counters.
       Only counters whose names are found in registeredCounters will be
       paid attention to
    """
    for counter in counters:
      if counter in self.registeredCounters:
        del self.registeredCounters[counter]

  def getRegisteredCounters(self):
    """Returns a list of the registered counters."""
    return keys(self.registeredCounters)

  def getCounterValue(self, counterName):
    """Returns the last value of the counter 'counterName'"""
    try:
      if counterName is "% Processor Time":
        return self._getCounterAverage(counterName)
      else:
        return self.registeredCounters[counterName][1][-1]
    except:
      return None

  def _getCounterAverage(self, counterName):
    """Returns the average value of the counter 'counterName'"""
    try:
      total = 0
      for v in self.registeredCounters[counterName][1]:
        total += v
      return total / len(self.registeredCounters[counterName][1])
    except:
      return None

  def getProcess(self):
    """Returns the process currently associated with this CounterManager"""
    return self.process

  def startMonitor(self):
    """Starts the monitoring process.
       Throws an exception if any error occurs
    """
    # TODO: make this function less ugly
    try:
      # the last process is the useful one
      self.pid = ffprocess.GetPidsByName(self.process)[-1]
      self.runThread = True
      self.start()
    except:
      print 'WARNING: problem starting counter monitor'

  def stopMonitor(self):
    """Stops the monitor"""
    # TODO: should probably wait until we know run() is completely stopped
    # before setting self.pid to None. Use a lock?
    self.runThread = False

  def run(self):
    """Performs the actual monitoring of the process. Will keep running
       until stopMonitor() is called
    """
    while self.runThread:
      for counter in self.registeredCounters.keys():
        # counter[0] is a function that gets the current value for
        # a counter
        # counter[1] is a list of recorded values
        try:
          self.registeredCounters[counter][1].append(
            self.registeredCounters[counter][0](self.pid))
        except:
          # if a counter throws an exception, remove it
          self.unregisterCounters([counter])

      time.sleep(self.pollInterval)
