# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import platform
import random
import re
import tempfile
import time
import traceback

from mozprofile import Profile

from tps.firefoxrunner import TPSFirefoxRunner
from tps.phase import TPSTestPhase
from tps.mozhttpd import MozHttpd

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
                            'extensions.getAddons.get.url': 'http://127.0.0.1:4567/en-US/firefox/api/%API_VERSION%/search/guid:%IDS%',
                            'extensions.update.enabled'    : False,
                            'extensions.update.notifyUser' : False,
                            'browser.shell.checkDefaultBrowser' : False,
                            'browser.tabs.warnOnClose' : False,
                            'browser.warnOnQuit': False,
                            'browser.sessionstore.resume_from_crash': False,
                            'services.sync.addons.ignoreRepositoryChecking': True,
                            'services.sync.firstSync': 'notReady',
                            'services.sync.lastversion': '1.0',
                            'services.sync.log.rootLogger': 'Trace',
                            'services.sync.log.logger.engine.addons': 'Trace',
                            'services.sync.log.logger.service.main': 'Trace',
                            'services.sync.log.logger.engine.bookmarks': 'Trace',
                            'services.sync.log.appender.console': 'Trace',
                            'services.sync.log.appender.debugLog.enabled': True,
                            'toolkit.startup.max_resumed_crashes': -1,
                            'browser.dom.window.dump.enabled': True,
                            # Allow installing extensions dropped into the profile folder
                            'extensions.autoDisableScopes': 10,
                            # Don't open a dialog to show available add-on updates
                            'extensions.update.notifyUser' : False,
                          }
    syncVerRe = re.compile(
        r"Sync version: (?P<syncversion>.*)\n")
    ffVerRe = re.compile(
        r"Firefox version: (?P<ffver>.*)\n")
    ffDateRe = re.compile(
        r"Firefox builddate: (?P<ffdate>.*)\n")

    def __init__(self, extensionDir,
                 testfile="sync.test",
                 binary=None, config=None, rlock=None, mobile=False,
                 logfile="tps.log", resultfile="tps_result.json",
                 ignore_unused_engines=False):
        self.extensions = []
        self.testfile = testfile
        self.logfile = os.path.abspath(logfile)
        self.resultfile = resultfile
        self.binary = binary
        self.ignore_unused_engines = ignore_unused_engines
        self.config = config if config else {}
        self.repo = None
        self.changeset = None
        self.branch = None
        self.numfailed = 0
        self.numpassed = 0
        self.nightly = False
        self.rlock = rlock
        self.mobile = mobile
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

    def writeToResultFile(self, postdata, body=None,
                          sendTo=['crossweave@mozilla.com']):
        """Writes results to test file"""

        results = {'results': []}

        if os.access(self.resultfile, os.F_OK):
            f = open(self.resultfile, 'r')
            results = json.loads(f.read())
            f.close()

        f = open(self.resultfile, 'w')
        if body is not None:
            postdata['body'] = body
        if self.numpassed is not None:
            postdata['numpassed'] = self.numpassed
        if self.numfailed is not None:
            postdata['numfailed'] = self.numfailed
        if self.firefoxRunner and self.firefoxRunner.url:
            postdata['firefoxrunnerurl'] = self.firefoxRunner.url

        postdata['sendTo'] = sendTo
        results['results'].append(postdata)
        f.write(json.dumps(results, indent=2))
        f.close()

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

    def run_single_test(self, testdir, testname):
        testpath = os.path.join(testdir, testname)
        self.log("Running test %s\n" % testname)

        # Create a random account suffix that is used when creating test
        # accounts on a staging server.
        #account_suffix = {"account-suffix": ''.join([str(random.randint(0,9))
        #                                             for i in range(1,6)])}
        #self.config['sync_account'].update(account_suffix)

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
            phaselist.append(TPSTestPhase(
                phase,
                profiles[profilename],
                testname,
                tmpfile.filename,
                self.logfile,
                self.env,
                self.firefoxRunner,
                self.log,
                ignore_unused_engines=self.ignore_unused_engines))

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
                                with open(weavelog, 'r') as fh:
                                    for line in fh:
                                        possible_time = line[0:13]
                                        if len(possible_time) == 13 and possible_time.isdigit():
                                            time_ms = int(possible_time)
                                            formatted = time.strftime('%Y-%m-%d %H:%M:%S',
                                                    time.localtime(time_ms / 1000))
                                            self.log('%s.%03d %s' % (
                                                formatted, time_ms % 1000, line[14:] ))
                                        else:
                                            self.log(line)
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

        try:
            repoinfo = self.firefoxRunner.runner.get_repositoryInfo()
        except:
            repoinfo = {}
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
            try:
                self.writeToResultFile(self.postdata,
                                       '<pre>%s</pre>' % traceback.format_exc())
            except:
                traceback.print_exc()
        else:
            try:

                if self.numfailed > 0 or self.numpassed == 0:
                    To = self.config['email'].get('notificationlist')
                else:
                    To = self.config['email'].get('passednotificationlist')
                self.writeToResultFile(self.postdata,
                                       sendTo=To)
            except:
                traceback.print_exc()
                try:
                    self.writeToResultFile(self.postdata,
                                           '<pre>%s</pre>' % traceback.format_exc())
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
        self.extensions.append(os.path.join(self.extensionDir, 'tps'))
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

        self.mozhttpd = MozHttpd(port=4567, docroot=testdir)
        self.mozhttpd.start()

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

        self.mozhttpd.stop()

        # generate the postdata we'll use to post the results to the db
        self.postdata = { 'tests': self.results,
                          'os':os_string,
                          'testtype': 'crossweave',
                          'productversion': self.productversion,
                          'addonversion': self.addonversion,
                          'synctype': self.synctype,
                        }
