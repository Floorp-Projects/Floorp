# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath
import shutil
import sys
import tempfile
import threading
import traceback

try:
    import json
except ImportError:
    import simplejson as json

here = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(0, here)

from b2gautomation import B2GDesktopAutomation
from runtests import Mochitest
from runtests import MochitestUtilsMixin
from runtests import MochitestOptions
from runtests import MochitestServer
from mochitest_options import B2GOptions, MochitestOptions

from marionette import Marionette

from mozdevice import DeviceManagerADB
from mozprofile import Profile, Preferences
from mozrunner import B2GRunner
import mozlog
import mozinfo
import moznetwork

log = mozlog.getLogger('Mochitest')

class B2GMochitest(MochitestUtilsMixin):
    def __init__(self, marionette,
                       out_of_process=True,
                       profile_data_dir=None,
                       locations=os.path.join(here, 'server-locations.txt')):
        super(B2GMochitest, self).__init__()
        self.marionette = marionette
        self.out_of_process = out_of_process
        self.locations = locations
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
        if len(self.urlOpts) > 0:
            test_url += "?" + "&".join(self.urlOpts)
        self.test_script_args.append(test_url)

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
            'locations': self.locations,
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

        self.leak_report_file = os.path.join(options.profilePath, "runtests_leaks.log")
        manifest = self.build_profile(options)

        self.startWebServer(options)
        self.startWebSocketServer(options, None)
        self.buildURLOptions(options, {'MOZ_HIDE_RESULTS_TABLE': '1'})

        if options.timeout:
            timeout = options.timeout + 30
        elif options.debugger or not options.autorun:
            timeout = None
        else:
            timeout = 330.0 # default JS harness timeout is 300 seconds

        log.info("runtestsb2g.py | Running tests: start.")
        status = 0
        try:
            runner_args = { 'profile': self.profile,
                            'devicemanager': self._dm,
                            'marionette': self.marionette,
                            'remote_test_root': self.remote_test_root,
                            'test_script': self.test_script,
                            'test_script_args': self.test_script_args }
            self.runner = B2GRunner(**runner_args)
            self.runner.start(outputTimeout=timeout)
            self.runner.wait()
        except KeyboardInterrupt:
            log.info("runtests.py | Received keyboard interrupt.\n");
            status = -1
        except:
            traceback.print_exc()
            log.error("runtests.py | Received unexpected exception while running application\n")
            status = 1

        self.stopWebServer(options)
        self.stopWebSocketServer(options)

        log.info("runtestsb2g.py | Running tests: end.")

        if manifest is not None:
            self.cleanup(manifest, options)
        return status


class B2GDeviceMochitest(B2GMochitest):

    _dm = None

    def __init__(self, marionette, devicemanager, profile_data_dir,
                 local_binary_dir, remote_test_root=None, remote_log_file=None):
        B2GMochitest.__init__(self, marionette, out_of_process=True, profile_data_dir=profile_data_dir)
        self._dm = devicemanager
        self.remote_test_root = remote_test_root or self._dm.getDeviceRoot()
        self.remote_profile = posixpath.join(self.remote_test_root, 'profile')
        self.remote_log = remote_log_file or posixpath.join(self.remote_test_root, 'log', 'mochitest.log')
        self.local_log = None
        self.local_binary_dir = local_binary_dir

        if not self._dm.dirExists(posixpath.dirname(self.remote_log)):
            self._dm.mkDirs(self.remote_log)

    def cleanup(self, manifest, options):
        if self.local_log:
            self._dm.getFile(self.remote_log, self.local_log)
            self._dm.removeFile(self.remote_log)

        if options.pidFile != "":
            try:
                os.remove(options.pidFile)
                os.remove(options.pidFile + ".xpcshell.pid")
            except:
                print "Warning: cleaning up pidfile '%s' was unsuccessful from the test harness" % options.pidFile

        # stop and clean up the runner
        if getattr(self, 'runner', False):
            self.runner.cleanup()
            self.runner = None

    def startWebServer(self, options):
        """ Create the webserver on the host and start it up """
        d = vars(options).copy()
        d['xrePath'] = self.local_binary_dir
        d['utilityPath'] = self.local_binary_dir
        d['profilePath'] = tempfile.mkdtemp()
        if d.get('httpdPath') is None:
            d['httpdPath'] = os.path.abspath(os.path.join(self.local_binary_dir, 'components'))
        self.server = MochitestServer(None, d)
        self.server.start()

        if (options.pidFile != ""):
            f = open(options.pidFile + ".xpcshell.pid", 'w')
            f.write("%s" % self.server._process.pid)
            f.close()
        self.server.ensureReady(90)

    def stopWebServer(self, options):
        if hasattr(self, 'server'):
            self.server.stop()

    def buildURLOptions(self, options, env):
        self.local_log = options.logFile
        options.logFile = self.remote_log
        options.profilePath = self.profile.profile
        retVal = super(B2GDeviceMochitest, self).buildURLOptions(options, env)

        self.setup_common_options(options)

        options.profilePath = self.remote_profile
        options.logFile = self.local_log
        return retVal


class B2GDesktopMochitest(B2GMochitest, Mochitest):

    def __init__(self, automation, marionette, profile_data_dir):
        B2GMochitest.__init__(self, marionette, out_of_process=False, profile_data_dir=profile_data_dir)
        Mochitest.__init__(self, automation)

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
        thread = threading.Thread(target=self.runMarionetteScript,
                                  args=(self.marionette,
                                        self.test_script,
                                        self.test_script_args))
        thread.start()

    def buildURLOptions(self, options, env):
        retVal = super(B2GDesktopMochitest, self).buildURLOptions(options, env)

        self.setup_common_options(options)

        # Copy the extensions to the B2G bundles dir.
        extensionDir = os.path.join(options.profilePath, 'extensions', 'staged')
        bundlesDir = os.path.join(os.path.dirname(options.app),
                                  'distribution', 'bundles')

        for filename in os.listdir(extensionDir):
            shutil.rmtree(os.path.join(bundlesDir, filename), True)
            shutil.copytree(os.path.join(extensionDir, filename),
                            os.path.join(bundlesDir, filename))

        return retVal

    def buildProfile(self, options):
        return self.build_profile(options)


def run_remote_mochitests(parser, options):
    # create our Marionette instance
    kwargs = {}
    if options.emulator:
        kwargs['emulator'] = options.emulator
        if options.noWindow:
            kwargs['noWindow'] = True
        if options.geckoPath:
            kwargs['gecko_path'] = options.geckoPath
        if options.logcat_dir:
            kwargs['logcat_dir'] = options.logcat_dir
        if options.busybox:
            kwargs['busybox'] = options.busybox
        if options.symbolsPath:
            kwargs['symbols_path'] = options.symbolsPath
    # needless to say sdcard is only valid if using an emulator
    if options.sdcard:
        kwargs['sdcard'] = options.sdcard
    if options.b2gPath:
        kwargs['homedir'] = options.b2gPath
    if options.marionette:
        host, port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)

    marionette = Marionette.getMarionetteOrExit(**kwargs)

    # create the DeviceManager
    kwargs = {'adbPath': options.adbPath,
              'deviceRoot': options.remoteTestRoot}
    if options.deviceIP:
        kwargs.update({'host': options.deviceIP,
                       'port': options.devicePort})
    dm = DeviceManagerADB(**kwargs)
    options = parser.verifyRemoteOptions(options)
    if (options == None):
        print "ERROR: Invalid options specified, use --help for a list of valid options"
        sys.exit(1)

    mochitest = B2GDeviceMochitest(marionette, dm, options.profile_data_dir, options.xrePath,
                                   remote_test_root=options.remoteTestRoot,
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
        mochitest.stopWebServer(options)
        mochitest.stopWebSocketServer(options)
        try:
            mochitest.cleanup(None, options)
        except:
            pass
        retVal = 1

    sys.exit(retVal)

def run_desktop_mochitests(parser, options):
    automation = B2GDesktopAutomation()

    # create our Marionette instance
    kwargs = {}
    if options.marionette:
        host, port = options.marionette.split(':')
        kwargs['host'] = host
        kwargs['port'] = int(port)
    marionette = Marionette.getMarionetteOrExit(**kwargs)
    automation.marionette = marionette

    mochitest = B2GDesktopMochitest(automation, marionette, options.profile_data_dir)

    # b2g desktop builds don't always have a b2g-bin file
    if options.app[-4:] == '-bin':
        options.app = options.app[:-4]

    options = MochitestOptions.verifyOptions(parser, options, mochitest)
    if options == None:
        sys.exit(1)

    if options.desktop and not options.profile:
        raise Exception("must specify --profile when specifying --desktop")

    automation.setServerInfo(options.webServer,
                             options.httpPort,
                             options.sslPort,
                             options.webSocketPort)
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
