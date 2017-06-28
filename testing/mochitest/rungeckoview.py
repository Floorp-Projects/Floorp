# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import posixpath
import shutil
import sys
import tempfile
import time
import traceback
from optparse import OptionParser

import mozcrash
import mozdevice
import mozlog
from mozprofile import Profile


class GeckoviewOptions(OptionParser):
    def __init__(self):
        OptionParser.__init__(self)
        self.add_option("--utility-path",
                        action="store", type="string", dest="utility_path",
                        default=None,
                        help="absolute path to directory containing utility programs")
        self.add_option("--symbols-path",
                        action="store", type="string", dest="symbols_path",
                        default=None,
                        help="absolute path to directory containing breakpad symbols, \
                              or the URL of a zip file containing symbols")
        self.add_option("--appname",
                        action="store", type="string", dest="app",
                        default="org.mozilla.geckoview_example",
                        help="geckoview_example package name")
        self.add_option("--deviceIP",
                        action="store", type="string", dest="deviceIP",
                        default=None,
                        help="ip address of remote device to test")
        self.add_option("--deviceSerial",
                        action="store", type="string", dest="deviceSerial",
                        default=None,
                        help="serial ID of remote device to test")
        self.add_option("--adbpath",
                        action="store", type="string", dest="adbPath",
                        default="adb",
                        help="Path to adb binary.")
        self.add_option("--remoteTestRoot",
                        action="store", type="string", dest="remoteTestRoot",
                        default=None,
                        help="remote directory to use as test root \
                              (eg. /mnt/sdcard/tests or /data/local/tests)")


class GeckoviewTestRunner:
    """
       A quick-and-dirty test harness to verify the geckoview_example
       app starts without crashing.
    """

    def __init__(self, log, dm, options):
        self.log = log
        self.dm = dm
        self.options = options
        self.appname = self.options.app.split('/')[-1]
        self.logcat = None
        self.build_profile()
        self.log.debug("options=%s" % vars(options))

    def build_profile(self):
        test_root = self.dm.deviceRoot
        self.remote_profile = posixpath.join(test_root, 'gv-profile')
        self.dm.mkDirs(posixpath.join(self.remote_profile, "x"))
        profile = Profile()
        self.dm.pushDir(profile.profile, self.remote_profile)
        self.log.debug("profile %s -> %s" %
                       (str(profile.profile), str(self.remote_profile)))

    def installed(self):
        """
        geckoview_example installed
        """
        installed = self.dm.shellCheckOutput(['pm', 'list', 'packages', self.appname])
        if self.appname not in installed:
            return (False, "%s not installed" % self.appname)
        return (True, "%s installed" % self.appname)

    def start(self):
        """
        geckoview_example starts
        """
        try:
            self.dm.stopApplication(self.appname)
            self.dm.recordLogcat()
            cmd = ['am', 'start', '-a', 'android.intent.action.MAIN', '-n',
                   'org.mozilla.geckoview_example/org.mozilla.geckoview_example.GeckoViewActivity',
                   '--es', 'args', '-profile %s' % self.remote_profile]
            env = {}
            env["MOZ_CRASHREPORTER"] = 1
            env["MOZ_CRASHREPORTER_NO_REPORT"] = 1
            env["XPCOM_DEBUG_BREAK"] = "stack"
            env["DISABLE_UNSAFE_CPOW_WARNINGS"] = 1
            env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = 1
            env["MOZ_IN_AUTOMATION"] = 1
            env["R_LOG_VERBOSE"] = 1
            env["R_LOG_LEVEL"] = 6
            env["R_LOG_DESTINATION"] = "stderr"
            i = 0
            for key, value in env.iteritems():
                cmd.append("--es")
                cmd.append("env%d" % i)
                cmd.append("%s=%s" % (key, str(value)))
                i = i + 1
            self.dm.shellCheckOutput(cmd)
        except mozdevice.DMError:
            return (False, "Exception during %s startup" % self.appname)
        return (True, "%s started" % self.appname)

    def started(self):
        """
        startup logcat messages
        """
        expected = [
            "zerdatime",
            "Displayed %s/.GeckoViewActivity" % self.appname
        ]
        # wait up to 60 seconds for startup
        for wait_time in xrange(60):
            time.sleep(1)
            self.logcat = self.dm.getLogcat()
            for line in self.logcat:
                for e in expected:
                    if e in line:
                        self.log.debug(line.strip())
                        expected.remove(e)
            if len(expected) == 0:
                return (True, "All expected logcat messages found")
        for e in expected:
            self.log.error("missing from logcat: '%s'" % e)
        return (False, "'%s' not found in logcat" % expected[0])

    def run_tests(self):
        """
           Run simple tests to verify that the geckoview_example app starts.
        """
        all_tests = [self.installed, self.start, self.started]
        self.log.suite_start(all_tests)
        pass_count = 0
        fail_count = 0
        for test in all_tests:
            self.test_name = test.__doc__.strip()
            self.log.test_start(self.test_name)

            expected = 'PASS'
            (passed, message) = test()
            if passed:
                pass_count = pass_count + 1
            else:
                fail_count = fail_count + 1
            status = 'PASS' if passed else 'FAIL'

            self.log.test_end(self.test_name, status, expected, message)

        crashed = self.check_for_crashes()
        if crashed:
            fail_count = 1
        else:
            self.log.info("Passed: %d" % pass_count)
            self.log.info("Failed: %d" % fail_count)
        self.log.suite_end()

        return 1 if fail_count else 0

    def check_for_crashes(self):
        if self.logcat:
            if mozcrash.check_for_java_exception(self.logcat, self.test_name):
                return True
        symbols_path = self.options.symbols_path
        try:
            dump_dir = tempfile.mkdtemp()
            remote_dir = posixpath.join(self.remote_profile, 'minidumps')
            if not self.dm.dirExists(remote_dir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when the app
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print "Automation Error: No crash directory (%s) found on remote device" % \
                    remote_dir
                # Whilst no crash was found, the run should still display as a failure
                return True
            self.dm.getDirectory(remote_dir, dump_dir)
            crashed = mozcrash.log_crashes(self.log, dump_dir, symbols_path, test=self.test_name)
        finally:
            try:
                shutil.rmtree(dump_dir)
            except:
                self.log.warn("unable to remove directory: %s" % dump_dir)
        return crashed

    def cleanup(self):
        """
           Cleanup at end of job run.
        """
        self.log.debug("Cleaning up...")
        self.dm.stopApplication(self.appname)
        self.dm.removeDir(self.remote_profile)
        self.log.debug("Cleanup complete.")


def run_test_harness(log, parser, options):
    device_args = {'deviceRoot': options.remoteTestRoot}
    device_args['adbPath'] = options.adbPath
    if options.deviceIP:
        device_args['host'] = options.deviceIP
        device_args['port'] = options.devicePort
    elif options.deviceSerial:
        device_args['deviceSerial'] = options.deviceSerial
    device_args['packageName'] = options.app
    dm = mozdevice.DroidADB(**device_args)

    runner = GeckoviewTestRunner(log, dm, options)
    result = -1
    try:
        result = runner.run_tests()
    except KeyboardInterrupt:
        log.info("rungeckoview.py | Received keyboard interrupt")
        result = -1
    except:
        traceback.print_exc()
        log.error(
            "rungeckoview.py | Received unexpected exception while running tests")
        result = 1
    finally:
        try:
            runner.cleanup()
        except mozdevice.DMError:
            # ignore device error while cleaning up
            pass
    return result


def main(args=sys.argv[1:]):
    parser = GeckoviewOptions()
    mozlog.commandline.add_logging_group(parser)
    options, args = parser.parse_args()
    if args:
        print >>sys.stderr, """Usage: %s""" % sys.argv[0]
        sys.exit(1)
    log = mozlog.commandline.setup_logging("rungeckoview", options,
                                           {"tbpl": sys.stdout})
    return run_test_harness(log, parser, options)


if __name__ == "__main__":
    sys.exit(main())
