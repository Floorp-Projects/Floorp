# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import posixpath
import shutil
import sys
import tempfile
import threading
import traceback

here = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, here)

from runtests import Mochitest
from runtests import MochitestUtilsMixin
from runtests import MochitestServer
from mochitest_options import B2GOptions, MochitestOptions

from marionette import Marionette

from mozdevice import DeviceManagerADB
from mozprofile import Profile, Preferences
import mozlog
import mozinfo

log = mozlog.getLogger('Mochitest')

class B2GMochitest(MochitestUtilsMixin):
    marionette = None

    def __init__(self, marionette_args,
                       out_of_process=True,
                       profile_data_dir=None,
                       locations=os.path.join(here, 'server-locations.txt')):
        super(B2GMochitest, self).__init__()
        self.marionette_args = marionette_args
        self.out_of_process = out_of_process
        self.locations_file = locations
        self.preferences = []
        self.webapps = None
        self.test_script = os.path.join(here, 'b2g_start_script.js')
        self.test_script_args = [self.out_of_process]
        self.product = 'b2g'

        if profile_data_dir:
            self.preferences = [os.path.join(profile_data_dir, f)
                                 for f in os.listdir(profile_data_dir) if f.startswith('pref')]
            self.webapps = [os.path.join(profile_data_dir, f)
                             for f in os.listdir(profile_data_dir) if f.startswith('webapp')]

        # mozinfo is populated by the parent class
        if mozinfo.info['debug']:
            self.SERVER_STARTUP_TIMEOUT = 180
        else:
            self.SERVER_STARTUP_TIMEOUT = 90

    def setup_common_options(self, options):
        test_url = self.buildTestPath(options)
        # For B2G emulators buildURLOptions has been called
        # without calling buildTestPath first and that
        # causes manifestFile not to be set
        if not "manifestFile=tests.json" in self.urlOpts:
            self.urlOpts.append("manifestFile=%s" % options.manifestFile)

        if len(self.urlOpts) > 0:
            test_url += "?" + "&".join(self.urlOpts)
        self.test_script_args.append(test_url)

    def buildTestPath(self, options, testsToFilter=None):
        if options.manifestFile != 'tests.json':
            super(B2GMochitest, self).buildTestPath(options, testsToFilter, disabled=False)
        return self.buildTestURL(options)

    def build_profile(self, options):
        # preferences
        prefs = {}
        for path in self.preferences:
            prefs.update(Preferences.read_prefs(path))

        for v in options.extraPrefs:
            thispref = v.split("=", 1)
            if len(thispref) < 2:
                print "Error: syntax error in --setpref=" + v
                sys.exit(1)
            prefs[thispref[0]] = thispref[1]

        # interpolate the preferences
        interpolation = { "server": "%s:%s" % (options.webServer, options.httpPort),
                          "OOP": "true" if self.out_of_process else "false" }
        prefs = json.loads(json.dumps(prefs) % interpolation)
        for pref in prefs:
            prefs[pref] = Preferences.cast(prefs[pref])

        kwargs = {
            'addons': self.getExtensionsToInstall(options),
            'apps': self.webapps,
            'locations': self.locations_file,
            'preferences': prefs,
            'proxy': {"remote": options.webServer}
        }

        if options.profile:
            self.profile = Profile.clone(options.profile, **kwargs)
        else:
            self.profile = Profile(**kwargs)

        options.profilePath = self.profile.profile
        # TODO bug 839108 - mozprofile should probably handle this
        manifest = self.addChromeToProfile(options)
        self.copyExtraFilesToProfile(options)
        return manifest

    def run_tests(self, options):
        """ Prepare, configure, run tests and cleanup """

        manifest = self.build_profile(options)
        self.leak_report_file = os.path.join(options.profilePath, "runtests_leaks.log")

        if options.debugger or not options.autorun:
            timeout = None
        else:
            if not options.timeout:
                if mozinfo.info['debug']:
                    options.timeout = 420
                else:
                    options.timeout = 300
            timeout = options.timeout + 30.0

        log.info("runtestsb2g.py | Running tests: start.")
        status = 0
        try:
            self.marionette_args['profile'] = self.profile
            self.marionette = Marionette(**self.marionette_args)
            self.runner = self.marionette.runner
            self.app_ctx = self.runner.app_ctx

            self.remote_log = posixpath.join(self.app_ctx.remote_test_root,
                                             'log', 'mochitest.log')
            if not self.app_ctx.dm.dirExists(posixpath.dirname(self.remote_log)):
                self.app_ctx.dm.mkDirs(self.remote_log)

            self.startServers(options, None)
            self.buildURLOptions(options, {'MOZ_HIDE_RESULTS_TABLE': '1'})
            self.test_script_args.append(not options.emulator)
            self.test_script_args.append(options.wifi)


            self.runner.start(outputTimeout=timeout)

            self.marionette.wait_for_port()
            self.marionette.start_session()
            self.marionette.set_context(self.marionette.CONTEXT_CHROME)

            # Disable offline status management (bug 777145), otherwise the network
            # will be 'offline' when the mochitests start.  Presumably, the network
            # won't be offline on a real device, so we only do this for emulators.
            self.marionette.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                Services.io.manageOfflineStatus = false;
                Services.io.offline = false;
                """)

            if os.path.isfile(self.test_script):
                with open(self.test_script, 'r') as script:
                    self.marionette.execute_script(script.read(),
                                                   script_args=self.test_script_args)
            else:
                self.marionette.execute_script(self.test_script,
                                               script_args=self.test_script_args)
            status = self.runner.wait()
            if status is None:
                # the runner has timed out
                status = 124
        except KeyboardInterrupt:
            log.info("runtests.py | Received keyboard interrupt.\n");
            status = -1
        except:
            traceback.print_exc()
            log.error("Automation Error: Received unexpected exception while running application\n")
            if hasattr(self, 'runner'):
                self.runner.check_for_crashes()
            status = 1

        self.stopServers()

        log.info("runtestsb2g.py | Running tests: end.")

        if manifest is not None:
            self.cleanup(manifest, options)
        return status


class B2GDeviceMochitest(B2GMochitest, Mochitest):
    remote_log = None

    def __init__(self, marionette_args, profile_data_dir,
                 local_binary_dir, remote_test_root=None, remote_log_file=None):
        B2GMochitest.__init__(self, marionette_args, out_of_process=True, profile_data_dir=profile_data_dir)
        self.local_log = None
        self.local_binary_dir = local_binary_dir

    def cleanup(self, manifest, options):
        if self.local_log:
            self.app_ctx.dm.getFile(self.remote_log, self.local_log)
            self.app_ctx.dm.removeFile(self.remote_log)

        if options.pidFile != "":
            try:
                os.remove(options.pidFile)
                os.remove(options.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % options.pidFile

        # stop and clean up the runner
        if getattr(self, 'runner', False):
            if self.local_log:
                self.app_ctx.dm.getFile(self.remote_log, self.local_log)
                self.app_ctx.dm.removeFile(self.remote_log)

            self.runner.cleanup()
            self.runner = None

    def startServers(self, options, debuggerInfo):
        """ Create the servers on the host and start them up """
        savedXre = options.xrePath
        savedUtility = options.utilityPath
        savedProfie = options.profilePath
        options.xrePath = self.local_binary_dir
        options.utilityPath = self.local_binary_dir
        options.profilePath = tempfile.mkdtemp()

        MochitestUtilsMixin.startServers(self, options, debuggerInfo)

        options.xrePath = savedXre
        options.utilityPath = savedUtility
        options.profilePath = savedProfie

    def buildURLOptions(self, options, env):
        self.local_log = options.logFile
        options.logFile = self.remote_log
        options.profilePath = self.profile.profile
        super(B2GDeviceMochitest, self).buildURLOptions(options, env)

        self.setup_common_options(options)

        options.profilePath = self.app_ctx.remote_profile
        options.logFile = self.local_log


class B2GDesktopMochitest(B2GMochitest, Mochitest):

    def __init__(self, marionette_args, profile_data_dir):
        B2GMochitest.__init__(self, marionette_args, out_of_process=False, profile_data_dir=profile_data_dir)
        Mochitest.__init__(self)
        self.certdbNew = True

    def runMarionetteScript(self, marionette, test_script, test_script_args):
        assert(marionette.wait_for_port())
        marionette.start_session()
        marionette.set_context(marionette.CONTEXT_CHROME)

        if os.path.isfile(test_script):
            f = open(test_script, 'r')
            test_script = f.read()
            f.close()
        self.marionette.execute_script(test_script,
                                       script_args=test_script_args)

    def startTests(self):
        # This is run in a separate thread because otherwise, the app's
        # stdout buffer gets filled (which gets drained only after this
        # function returns, by waitForFinish), which causes the app to hang.
        self.marionette = Marionette(**self.marionette_args)
        thread = threading.Thread(target=self.runMarionetteScript,
                                  args=(self.marionette,
                                        self.test_script,
                                        self.test_script_args))
        thread.start()

    def buildURLOptions(self, options, env):
        super(B2GDesktopMochitest, self).buildURLOptions(options, env)

        self.setup_common_options(options)

        # Copy the extensions to the B2G bundles dir.
        extensionDir = os.path.join(options.profilePath, 'extensions', 'staged')
        bundlesDir = os.path.join(os.path.dirname(options.app),
                                  'distribution', 'bundles')

        for filename in os.listdir(extensionDir):
            shutil.rmtree(os.path.join(bundlesDir, filename), True)
            shutil.copytree(os.path.join(extensionDir, filename),
                            os.path.join(bundlesDir, filename))

    def buildProfile(self, options):
        return self.build_profile(options)


def run_remote_mochitests(parser, options):
    # create our Marionette instance
    marionette_args = {
        'adb_path': options.adbPath,
        'emulator': options.emulator,
        'no_window': options.noWindow,
        'logdir': options.logdir,
        'busybox': options.busybox,
        'symbols_path': options.symbolsPath,
        'sdcard': options.sdcard,
        'homedir': options.b2gPath,
    }
    if options.marionette:
        host, port = options.marionette.split(':')
        marionette_args['host'] = host
        marionette_args['port'] = int(port)

    options = parser.verifyRemoteOptions(options)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    mochitest = B2GDeviceMochitest(marionette_args, options.profile_data_dir, options.xrePath,
                                   remote_log_file=options.remoteLogFile)

    options = parser.verifyOptions(options, mochitest)
    if (options == None):
        sys.exit(1)

    retVal = 1
    try:
        mochitest.cleanup(None, options)
        retVal = mochitest.run_tests(options)
    except:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        mochitest.stopServers()
        try:
            mochitest.cleanup(None, options)
        except:
            pass
        retVal = 1

    sys.exit(retVal)

def run_desktop_mochitests(parser, options):
    # create our Marionette instance
    marionette_args = {}
    if options.marionette:
        host, port = options.marionette.split(':')
        marionette_args['host'] = host
        marionette_args['port'] = int(port)
    mochitest = B2GDesktopMochitest(marionette_args, options.profile_data_dir)

    # add a -bin suffix if b2g-bin exists, but just b2g was specified
    if options.app[-4:] != '-bin':
        if os.path.isfile("%s-bin" % options.app):
            options.app = "%s-bin" % options.app

    options = MochitestOptions.verifyOptions(parser, options, mochitest)
    if options == None:
        sys.exit(1)

    if options.desktop and not options.profile:
        raise Exception("must specify --profile when specifying --desktop")

    options.browserArgs += ['-marionette']

    sys.exit(mochitest.runTests(options, onLaunch=mochitest.startTests))

def main():
    parser = B2GOptions()
    options, args = parser.parse_args()

    if options.desktop:
        run_desktop_mochitests(parser, options)
    else:
        run_remote_mochitests(parser, options)

if __name__ == "__main__":
    main()
