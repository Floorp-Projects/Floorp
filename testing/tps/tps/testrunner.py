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

import httplib
import json
import os
import platform
import random
import re
import socket
import tempfile
import time
import traceback
import urllib

from threading import RLock

from mozprofile import Profile

from tps.firefoxrunner import TPSFirefoxRunner
from tps.phase import TPSTestPhase


class TempFile(object):
  """Class for temporary files that delete themselves when garbage-collected.
  """

  def __init__(self, prefix=None):
    self.fd, self.filename = self.tmpfile = tempfile.mkstemp(prefix=prefix)

  def write(self, data):
    if self.fd:
      os.write(self.fd, data)

  def close(self):
    if self.fd:
      os.close(self.fd)
      self.fd = None

  def cleanup(self):
    if self.fd:
      self.close()
    if os.access(self.filename, os.F_OK):
      os.remove(self.filename)

  __del__ = cleanup


class TPSTestRunner(object):

  default_env = { 'MOZ_CRASHREPORTER_DISABLE': '1',
                  'GNOME_DISABLE_CRASH_DIALOG': '1',
                  'XRE_NO_WINDOWS_CRASH_DIALOG': '1',
                  'MOZ_NO_REMOTE': '1',
                  'XPCOM_DEBUG_BREAK': 'warn',
                }
  default_preferences = { 'app.update.enabled' : False,
                          'extensions.update.enabled'    : False,
                          'extensions.update.notifyUser' : False,
                          'browser.shell.checkDefaultBrowser' : False,
                          'browser.tabs.warnOnClose' : False,
                          'browser.warnOnQuit': False,
                          'browser.sessionstore.resume_from_crash': False,
                          'services.sync.firstSync': 'notReady',
                          'services.sync.lastversion': '1.0',
                          'services.sync.log.rootLogger': 'Trace',
                          'services.sync.log.logger.service.main': 'Trace',
                          'services.sync.log.logger.engine.bookmarks': 'Trace',
                          'services.sync.log.appender.console': 'Trace',
                          'services.sync.log.appender.debugLog.enabled': True,
                          'browser.dom.window.dump.enabled': True,
                          'extensions.checkCompatibility.4.0': False,
                        }
  syncVerRe = re.compile(
      r"Weave version: (?P<syncversion>.*)\n")
  ffVerRe = re.compile(
      r"Firefox version: (?P<ffver>.*)\n")
  ffDateRe = re.compile(
      r"Firefox builddate: (?P<ffdate>.*)\n")

  def __init__(self, extensionDir, emailresults=False, testfile="sync.test",
               binary=None, config=None, rlock=None, mobile=False,
               autolog=False, logfile="tps.log"):
    self.extensions = []
    self.emailresults = emailresults
    self.testfile = testfile
    self.logfile = os.path.abspath(logfile)
    self.binary = binary
    self.config = config if config else {}
    self.repo = None
    self.changeset = None
    self.branch = None
    self.numfailed = 0
    self.numpassed = 0
    self.nightly = False
    self.rlock = rlock
    self.mobile = mobile
    self.autolog = autolog
    self.tpsxpi = None
    self.firefoxRunner = None
    self.extensionDir = extensionDir
    self.productversion = None
    self.addonversion = None
    self.postdata = {}
    self.errorlogs = {}

  @property
  def mobile(self):
    return self._mobile

  @mobile.setter
  def mobile(self, value):
    self._mobile = value
    self.synctype = 'desktop' if not self._mobile else 'mobile'

  def log(self, msg, printToConsole=False):
    """Appends a string to the logfile"""

    f = open(self.logfile, 'a')
    f.write(msg)
    f.close()
    if printToConsole:
      print msg

  def _zip_add_file(self, zip, file, rootDir):
    zip.write(os.path.join(rootDir, file), file)

  def _zip_add_dir(self, zip, dir, rootDir):
    try:
      zip.write(os.path.join(rootDir, dir), dir)
    except:
      # on some OS's, adding directory entries doesn't seem to work
      pass
    for root, dirs, files in os.walk(os.path.join(rootDir, dir)):
      for f in files:
        zip.write(os.path.join(root, f), os.path.join(dir, f))

  def make_xpi(self):
    """Build the test extension."""

    tpsdir = os.path.join(self.extensionDir, "tps")

    if self.tpsxpi is None:
      tpsxpi = os.path.join(tpsdir, "tps.xpi")

      if os.access(tpsxpi, os.F_OK):
        os.remove(tpsxpi)
      if not os.access(os.path.join(tpsdir, "install.rdf"), os.F_OK):
        raise Exception("extension code not found in %s" % tpsdir)

      from zipfile import ZipFile
      z = ZipFile(tpsxpi, 'w')
      self._zip_add_file(z, 'chrome.manifest', tpsdir)
      self._zip_add_file(z, 'install.rdf', tpsdir)
      self._zip_add_dir(z, 'components', tpsdir)
      self._zip_add_dir(z, 'modules', tpsdir)
      z.close()

      self.tpsxpi = tpsxpi

    return self.tpsxpi

  def run_single_test(self, testdir, testname):
    testpath = os.path.join(testdir, testname)
    self.log("Running test %s\n" % testname)

    # Create a random account suffix that is used when creating test
    # accounts on a staging server.
    account_suffix = {"account-suffix": ''.join([str(random.randint(0,9))
                                                 for i in range(1,6)])}
    self.config['account'].update(account_suffix)

    # Read and parse the test file, merge it with the contents of the config
    # file, and write the combined output to a temporary file.
    f = open(testpath, 'r')
    testcontent = f.read()
    f.close()
    try:
      test = json.loads(testcontent)
    except:
      test = json.loads(testcontent[testcontent.find("{"):testcontent.find("}") + 1])

    testcontent += 'var config = %s;\n' % json.dumps(self.config, indent=2)
    testcontent += 'var seconds_since_epoch = %d;\n' % int(time.time())

    tmpfile = TempFile(prefix='tps_test_')
    tmpfile.write(testcontent)
    tmpfile.close()

    # generate the profiles defined in the test, and a list of test phases
    profiles = {}
    phaselist = []
    for phase in test:
      profilename = test[phase]

      # create the profile if necessary
      if not profilename in profiles:
        profiles[profilename] = Profile(preferences = self.preferences,
                                        addons = self.extensions)

      # create the test phase
      phaselist.append(TPSTestPhase(phase,
                                    profiles[profilename],
                                    testname,
                                    tmpfile.filename,
                                    self.logfile,
                                    self.env,
                                    self.firefoxRunner,
                                    self.log))

    # sort the phase list by name
    phaselist = sorted(phaselist, key=lambda phase: phase.phase)

    # run each phase in sequence, aborting at the first failure
    for phase in phaselist:
      phase.run()

      # if a failure occurred, dump the entire sync log into the test log
      if phase.status != "PASS":
        for profile in profiles:
          self.log("\nDumping sync log for profile %s\n" %  profiles[profile].profile)
          for root, dirs, files in os.walk(os.path.join(profiles[profile].profile, 'weave', 'logs')):
            for f in files:
              weavelog = os.path.join(profiles[profile].profile, 'weave', 'logs', f)
              if os.access(weavelog, os.F_OK):
                f = open(weavelog, 'r')
                msg = f.read()
                self.log(msg)
                f.close()
              self.log("\n")
        break;

    # grep the log for FF and sync versions
    f = open(self.logfile)
    logdata = f.read()
    match = self.syncVerRe.search(logdata)
    sync_version = match.group("syncversion") if match else 'unknown'
    match = self.ffVerRe.search(logdata)
    firefox_version = match.group("ffver") if match else 'unknown'
    match = self.ffDateRe.search(logdata)
    firefox_builddate = match.group("ffdate") if match else 'unknown'
    f.close()
    if phase.status == 'PASS':
      logdata = ''
    else:
      # we only care about the log data for this specific test
      logdata = logdata[logdata.find('Running test %s' % (str(testname))):]

    result = {
      'PASS': lambda x: ('TEST-PASS', ''),
      'FAIL': lambda x: ('TEST-UNEXPECTED-FAIL', x.rstrip()),
      'unknown': lambda x: ('TEST-UNEXPECTED-FAIL', 'test did not complete')
    } [phase.status](phase.errline)
    logstr = "\n%s | %s%s\n" % (result[0], testname, (' | %s' % result[1] if result[1] else ''))

    repoinfo = self.firefoxRunner.get_respository_info()
    apprepo = repoinfo.get('application_repository', '')
    appchangeset = repoinfo.get('application_changeset', '')

    # save logdata to a temporary file for posting to the db
    tmplogfile = None
    if logdata:
      tmplogfile = TempFile(prefix='tps_log_')
      tmplogfile.write(logdata)
      tmplogfile.close()
      self.errorlogs[testname] = tmplogfile

    resultdata = ({ "productversion": { "version": firefox_version,
                                        "buildid": firefox_builddate,
                                        "builddate": firefox_builddate[0:8],
                                        "product": "Firefox",
                                        "repository": apprepo,
                                        "changeset": appchangeset,
                                      },
                    "addonversion": { "version": sync_version,
                                      "product": "Firefox Sync" },
                    "name": testname,
                    "message": result[1],
                    "state": result[0],
                    "logdata": logdata
                  })

    self.log(logstr, True)
    for phase in phaselist:
      print "\t%s: %s" % (phase.phase, phase.status)
      if phase.status == 'FAIL':
        break

    return resultdata

  def run_tests(self):
    # delete the logfile if it already exists
    if os.access(self.logfile, os.F_OK):
      os.remove(self.logfile)

    # Make a copy of the default env variables and preferences, and update
    # them for mobile settings if needed.
    self.env = self.default_env.copy()
    self.preferences = self.default_preferences.copy()
    if self.mobile:
      self.preferences.update({'services.sync.client.type' : 'mobile'})

    # Acquire a lock to make sure no other threads are running tests
    # at the same time.
    if self.rlock:
      self.rlock.acquire()

    try:
      # Create the Firefox runner, which will download and install the
      # build, as needed.
      if not self.firefoxRunner:
        self.firefoxRunner = TPSFirefoxRunner(self.binary)

      # now, run the test group
      self.run_test_group()

    except:
      traceback.print_exc()
      self.numpassed = 0
      self.numfailed = 1
      if self.emailresults:
        try:
          self.sendEmail('<pre>%s</pre>' % traceback.format_exc(),
                         sendTo='crossweave@mozilla.com')
        except:
          traceback.print_exc()
      else:
        raise

    else:
      try:
        if self.autolog:
          self.postToAutolog()
        if self.emailresults:
          self.sendEmail()
      except:
        traceback.print_exc()
        try:
          self.sendEmail('<pre>%s</pre>' % traceback.format_exc(),
                         sendTo='crossweave@mozilla.com')
        except:
          traceback.print_exc()

    # release our lock
    if self.rlock:
      self.rlock.release()

    # dump out a summary of test results
    print 'Test Summary\n'
    for test in self.postdata.get('tests', {}):
      print '%s | %s | %s' % (test['state'], test['name'], test['message'])

  def run_test_group(self):
    self.results = []
    self.extensions = []

    # set the OS we're running on
    os_string = platform.uname()[2] + " " + platform.uname()[3]
    if os_string.find("Darwin") > -1:
      os_string = "Mac OS X " + platform.mac_ver()[0]
    if platform.uname()[0].find("Linux") > -1:
      os_string = "Linux " + platform.uname()[5]
    if platform.uname()[0].find("Win") > -1:
      os_string = "Windows " + platform.uname()[3]

    # reset number of passed/failed tests
    self.numpassed = 0
    self.numfailed = 0

    # build our tps.xpi extension
    self.extensions.append(self.make_xpi())
    self.extensions.append(os.path.join(self.extensionDir, "mozmill"))

    # build the test list
    try:
      f = open(self.testfile)
      jsondata = f.read()
      f.close()
      testfiles = json.loads(jsondata)
      testlist = testfiles['tests']
    except ValueError:
      testlist = [os.path.basename(self.testfile)]
    testdir = os.path.dirname(self.testfile)

    # run each test, and save the results
    for test in testlist:
      result = self.run_single_test(testdir, test)

      if not self.productversion:
        self.productversion = result['productversion']
      if not self.addonversion:
        self.addonversion = result['addonversion']

      self.results.append({'state': result['state'], 
                           'name': result['name'], 
                           'message': result['message'],
                           'logdata': result['logdata']})
      if result['state'] == 'TEST-PASS':
        self.numpassed += 1
      else:
        self.numfailed += 1

    # generate the postdata we'll use to post the results to the db
    self.postdata = { 'tests': self.results, 
                      'os':os_string,
                      'testtype': 'crossweave',
                      'productversion': self.productversion,
                      'addonversion': self.addonversion,
                      'synctype': self.synctype,
                    }

  def sendEmail(self, body=None, sendTo=None):
    # send the result e-mail
    if self.config.get('email') and self.config['email'].get('username') \
       and self.config['email'].get('password'):

      from tps.sendemail import SendEmail
      from tps.emailtemplate import GenerateEmailBody

      if body is None:
        body = GenerateEmailBody(self.postdata, self.numpassed, self.numfailed, self.config['account']['serverURL'])

      subj = "TPS Report: "
      if self.numfailed == 0 and self.numpassed > 0:
        subj += "YEEEAAAHHH"
      else:
        subj += "PC LOAD LETTER"

      changeset = self.postdata['productversion']['changeset'] if \
          self.postdata and self.postdata.get('productversion') and \
          self.postdata['productversion'].get('changeset') \
          else 'unknown'
      subj +=", changeset " + changeset + "; " + str(self.numfailed) + \
             " failed, " + str(self.numpassed) + " passed"

      To = [sendTo] if sendTo else None
      if not To:
        if self.numfailed > 0 or self.numpassed == 0:
          To = self.config['email'].get('notificationlist')
        else:
          To = self.config['email'].get('passednotificationlist')

      if To:
        SendEmail(From=self.config['email']['username'],
                  To=To,
                  Subject=subj,
                  HtmlData=body,
                  Username=self.config['email']['username'],
                  Password=self.config['email']['password'])

  def postToAutolog(self):
    from mozautolog import RESTfulAutologTestGroup as AutologTestGroup

    group = AutologTestGroup(
              harness='crossweave',
              testgroup='crossweave-%s' % self.synctype,
              server=self.config.get('es'),
              restserver=self.config.get('restserver'),
              machine=socket.gethostname(),
              platform=self.config.get('platform', None),
              os=self.config.get('os', None),
            )
    tree = self.postdata['productversion']['repository']
    group.set_primary_product(
              tree=tree[tree.rfind("/")+1:],
              version=self.postdata['productversion']['version'],
              buildid=self.postdata['productversion']['buildid'],
              buildtype='opt',
              revision=self.postdata['productversion']['changeset'],
            )
    group.add_test_suite(
              passed=self.numpassed,
              failed=self.numfailed,
              todo=0,
            )
    for test in self.results:
      if test['state'] != "TEST-PASS":
        errorlog = self.errorlogs.get(test['name'])
        errorlog_filename = errorlog.filename if errorlog else None
        group.add_test_failure(
              test = test['name'],
              status = test['state'],
              text = test['message'],
              logfile = errorlog_filename
            )
    group.submit()

    # Iterate through all testfailure objects, and update the postdata
    # dict with the testfailure logurl's, if any.
    for tf in group.testsuites[-1].testfailures:
      result = [x for x in self.results if x.get('name') == tf.test]
      if not result:
        continue
      result[0]['logurl'] = tf.logurl

