# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re

class TPSTestPhase(object):

  lineRe = re.compile(
      r"^(.*?)test phase (?P<matchphase>\d+): (?P<matchstatus>.*)$")

  def __init__(self, phase, profile, testname, testpath, logfile, env,
               firefoxRunner, logfn, ignore_unused_engines=False):
    self.phase = phase
    self.profile = profile
    self.testname = str(testname) # this might be passed in as unicode
    self.testpath = testpath
    self.logfile = logfile
    self.env = env
    self.firefoxRunner = firefoxRunner
    self.log = logfn
    self.ignore_unused_engines = ignore_unused_engines
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

    if self.ignore_unused_engines:
        args.append('--ignore-unused-engines')

    self.log("\nlaunching Firefox for phase %s with args %s\n" %
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

