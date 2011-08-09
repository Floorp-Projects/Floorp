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
# The Original Code is TPS.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Griffin <jgriffin@mozilla.com>
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

import os
import re

class TPSTestPhase(object):

  lineRe = re.compile(
      r"^(.*?)test phase (?P<matchphase>\d+): (?P<matchstatus>.*)$")

  def __init__(self, phase, profile, testname, testpath, logfile, env,
               firefoxRunner, logfn):
    self.phase = phase
    self.profile = profile
    self.testname = str(testname) # this might be passed in as unicode
    self.testpath = testpath
    self.logfile = logfile
    self.env = env
    self.firefoxRunner = firefoxRunner
    self.log = logfn
    self._status = None
    self.errline = ''

  @property
  def phasenum(self):
    match = re.match('.*?(\d+)', self.phase)
    if match:
      return match.group(1)

  @property
  def status(self):
    return self._status if self._status else 'unknown'

  def run(self):
    # launch Firefox
    args = [ '-tps', self.testpath, 
             '-tpsphase', self.phasenum, 
             '-tpslogfile', self.logfile ]

    self.log("\nlaunching firefox for phase %s with args %s\n" % 
             (self.phase, str(args)))
    returncode = self.firefoxRunner.run(env=self.env,
                                        args=args, 
                                        profile=self.profile)

    # parse the logfile and look for results from the current test phase
    found_test = False
    f = open(self.logfile, 'r')
    for line in f:

      # skip to the part of the log file that deals with the test we're running
      if not found_test:
        if line.find("Running test %s" % self.testname) > -1:
          found_test = True
        else:
          continue

      # look for the status of the current phase
      match = self.lineRe.match(line)
      if match:
        if match.group("matchphase") == self.phasenum:
          self._status = match.group("matchstatus")
          break

      # set the status to FAIL if there is TPS error
      if line.find("CROSSWEAVE ERROR: ") > -1 and not self._status:
        self._status = "FAIL"
        self.errline = line[line.find("CROSSWEAVE ERROR: ") + len("CROSSWEAVE ERROR: "):]

    f.close()

