# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os

import mozpack.path as mozpath

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mozbuild.shellutil import (
    split as shell_split,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)


# NOTE python/mach/mach/commands/commandinfo.py references this function
#      by name. If this function is renamed or removed, that file should
#      be updated accordingly as well.
def REMOVED(cls):
    """Command no longer exists! Use the Gradle configuration rooted in the top source directory instead.

    See https://developer.mozilla.org/en-US/docs/Simple_Firefox_for_Android_build#Developing_Firefox_for_Android_in_Android_Studio_or_IDEA_IntelliJ.
    """
    return False


@CommandProvider
class MachCommands(MachCommandBase):
    @Command('android', category='devenv',
        description='Run Android-specific commands.',
        conditions=[conditions.is_android])
    def android(self):
        pass


    @SubCommand('android', 'test',
        """Run Android local unit tests.
        See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-test""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_test(self, args):
        gradle_targets = [
            'app:testOfficialPhotonDebugUnitTest',
        ]
        ret = self.gradle(gradle_targets + ["--continue"] + args, verbose=True)

        # Findbug produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import itertools
        import xml.etree.ElementTree as ET

        from mozpack.files import (
            FileFinder,
        )

        if 'TASK_ID' in os.environ and 'RUN_ID' in os.environ:
            root_url = "https://queue.taskcluster.net/v1/task/{}/runs/{}/artifacts/public/android/unittest".format(os.environ['TASK_ID'], os.environ['RUN_ID'])
        else:
            root_url = os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/tests')

        reports = ('officialPhotonDebug',)
        for report in reports:
            finder = FileFinder(os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/test-results/', report))
            for p, _ in finder.find('TEST-*.xml'):
                f = open(os.path.join(finder.base, p), 'rt')
                tree = ET.parse(f)
                root = tree.getroot()

                print('SUITE-START | android-test | {} {}'.format(report, root.get('name')))

                for testcase in root.findall('testcase'):
                    name = testcase.get('name')
                    print('TEST-START | {}'.format(name))

                    # Schema cribbed from
                    # http://llg.cubic.org/docs/junit/.  There's no
                    # particular advantage to formatting the error, so
                    # for now let's just output the unexpected XML
                    # tag.
                    error_count = 0
                    for unexpected in itertools.chain(testcase.findall('error'),
                                                      testcase.findall('failure')):
                        for line in ET.tostring(unexpected).strip().splitlines():
                            print('TEST-UNEXPECTED-FAIL | {} | {}'.format(name, line))
                        error_count += 1
                        ret |= 1

                    # Skipped tests aren't unexpected at this time; we
                    # disable some tests that require live remote
                    # endpoints.
                    for skipped in testcase.findall('skipped'):
                        for line in ET.tostring(skipped).strip().splitlines():
                            print('TEST-INFO | {} | {}'.format(name, line))

                    if not error_count:
                        print('TEST-PASS | {}'.format(name))

                print('SUITE-END | android-test | {} {}'.format(report, root.get('name')))

            title = report
            print("TinderboxPrint: report<br/><a href='{}/{}/index.html'>HTML {} report</a>, visit \"Inspect Task\" link for details".format(root_url, report, title))

        return ret


    @SubCommand('android', 'lint',
        """Run Android lint.
        See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-lint""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_lint(self, args):
        gradle_targets = [
            'app:lintOfficialPhotonDebug',
        ]
        ret = self.gradle(gradle_targets + ["--continue"] + args, verbose=True)

        # Android Lint produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        if 'TASK_ID' in os.environ and 'RUN_ID' in os.environ:
            root_url = "https://queue.taskcluster.net/v1/task/{}/runs/{}/artifacts/public/android/lint".format(os.environ['TASK_ID'], os.environ['RUN_ID'])
        else:
            root_url = os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports')

        reports = ('officialPhotonDebug',)
        for report in reports:
            f = open(os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/lint-results-{}.xml'.format(report)), 'rt')
            tree = ET.parse(f)
            root = tree.getroot()

            print('SUITE-START | android-lint | {}'.format(report))
            for issue in root.findall("issue[@severity='Error']"):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <issue> XML
                # tag.
                for line in ET.tostring(issue).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}'.format(line))
                ret |= 1
            print('SUITE-END | android-lint | {}'.format(report))

            title = report
            print("TinderboxPrint: report<br/><a href='{}/lint-results-{}.html'>HTML {} report</a>, visit \"Inspect Task\" link for details".format(root_url, report, title))
            print("TinderboxPrint: report<br/><a href='{}/lint-results-{}.xml'>XML {} report</a>, visit \"Inspect Task\" link for details".format(root_url, report, title))

        return ret


    @SubCommand('android', 'checkstyle',
        """Run Android checkstyle.
        See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-checkstyle""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_checkstyle(self, args):
        gradle_targets = [
            'app:checkstyle',
        ]
        ret = self.gradle(gradle_targets + ["--continue"] + args, verbose=True)

        # Checkstyle produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        f = open(os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/checkstyle/checkstyle.xml'), 'rt')
        tree = ET.parse(f)
        root = tree.getroot()

        print('SUITE-START | android-checkstyle')
        for file in root.findall('file'):
            name = file.get('name')

            print('TEST-START | {}'.format(name))
            error_count = 0
            for error in file.findall('error'):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <error> XML
                # tag.
                print('TEST-UNEXPECTED-FAIL | {}'.format(name))
                for line in ET.tostring(error).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}'.format(line))
                error_count += 1
                ret |= 1

            if not error_count:
                print('TEST-PASS | {}'.format(name))
        print('SUITE-END | android-checkstyle')

        # Now the reports, linkified.
        if 'TASK_ID' in os.environ and 'RUN_ID' in os.environ:
            root_url = "https://queue.taskcluster.net/v1/task/{}/runs/{}/artifacts/public/android/checkstyle".format(os.environ['TASK_ID'], os.environ['RUN_ID'])
        else:
            root_url = os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/checkstyle')

        print("TinderboxPrint: report<br/><a href='{}/checkstyle.html'>HTML checkstyle report</a>, visit \"Inspect Task\" link for details".format(root_url))
        print("TinderboxPrint: report<br/><a href='{}/checkstyle.xml'>XML checkstyle report</a>, visit \"Inspect Task\" link for details".format(root_url))

        return ret


    @SubCommand('android', 'findbugs',
        """Run Android findbugs.
        See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-findbugs""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_findbugs(self, dryrun=False, args=[]):
        gradle_targets = [
            'app:findbugsXmlOfficialPhotonDebug',
            'app:findbugsHtmlOfficialPhotonDebug',
        ]
        ret = self.gradle(gradle_targets + ["--continue"] + args, verbose=True)

        # Findbug produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        if 'TASK_ID' in os.environ and 'RUN_ID' in os.environ:
            root_url = "https://queue.taskcluster.net/v1/task/{}/runs/{}/artifacts/public/artifacts/findbugs".format(os.environ['TASK_ID'], os.environ['RUN_ID'])
        else:
            root_url = os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/findbugs')

        reports = ('findbugs-officialPhotonDebug-output.xml',)
        for report in reports:
            try:
                f = open(os.path.join(self.topobjdir, 'gradle/build/mobile/android/app/reports/findbugs', report), 'rt')
            except IOError:
                continue

            tree = ET.parse(f)
            root = tree.getroot()

            print('SUITE-START | android-findbugs | {}'.format(report))
            for error in root.findall('./BugInstance'):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <error> XML
                # tag.
                print('TEST-UNEXPECTED-FAIL | {}:{} | {}'.format(report, error.get('type'), error.find('Class').get('classname')))
                for line in ET.tostring(error).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}:{} | {}'.format(report, error.get('type'), line))
                ret |= 1
            print('SUITE-END | android-findbugs | {}'.format(report))

            title = report.replace('findbugs-', '').replace('-output.xml', '')
            print("TinderboxPrint: report<br/><a href='{}/{}'>HTML {} report</a>, visit \"Inspect Task\" link for details".format(root_url, report.replace('.xml', '.html'), title))
            print("TinderboxPrint: report<br/><a href='{}/{}'>XML {} report</a>, visit \"Inspect Task\" link for details".format(root_url, report, title))

        return ret


    @SubCommand('android', 'gradle-dependencies',
        """Collect Android Gradle dependencies.
        See http://firefox-source-docs.mozilla.org/build/buildsystem/toolchains.html#firefox-for-android-with-gradle""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_gradle_dependencies(self, args):
        # The union, plus a bit more, of all of the Gradle tasks
        # invoked by the android-* automation jobs.
        gradle_targets = [
            'app:checkstyle',
            'app:assembleOfficialPhotonRelease',
            'app:assembleOfficialPhotonDebug',
            'app:assembleOfficialPhotonDebugAndroidTest',
            'app:findbugsXmlOfficialPhotonDebug',
            'app:findbugsHtmlOfficialPhotonDebug',
            'app:lintOfficialPhotonDebug',
            # Does not include Gecko binaries -- see mobile/android/gradle/with_gecko_binaries.gradle.
            'geckoview:assembleWithoutGeckoBinaries',
            # So that we pick up the test dependencies for the builders.
            'geckoview_example:assembleWithoutGeckoBinaries',
            'geckoview_example:assembleWithoutGeckoBinariesAndroidTest',
        ]
        # We don't want to gate producing dependency archives on clean
        # lint or checkstyle, particularly because toolchain versions
        # can change the outputs for those processes.
        ret = self.gradle(gradle_targets + ["--continue"] + args, verbose=True)

        return 0


    @Command('gradle', category='devenv',
        description='Run gradle.',
        conditions=[conditions.is_android])
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the build is running.')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def gradle(self, args, verbose=False):
        if not verbose:
            # Avoid logging the command
            self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        # In automation, JAVA_HOME is set via mozconfig, which needs
        # to be specially handled in each mach command. This turns
        # $JAVA_HOME/bin/java into $JAVA_HOME.
        java_home = os.path.dirname(os.path.dirname(self.substs['JAVA']))

        gradle_flags = shell_split(self.substs.get('GRADLE_FLAGS', ''))

        # We force the Gradle JVM to run with the UTF-8 encoding, since we
        # filter strings.xml, which is really UTF-8; the ellipsis character is
        # replaced with ??? in some encodings (including ASCII).  It's not yet
        # possible to filter with encodings in Gradle
        # (https://github.com/gradle/gradle/pull/520) and it's challenging to
        # do our filtering with Gradle's Ant support.  Moreover, all of the
        # Android tools expect UTF-8: see
        # http://tools.android.com/knownissues/encoding.  See
        # http://stackoverflow.com/a/21267635 for discussion of this approach.
        #
        # It's not even enough to set the encoding just for Gradle; it
        # needs to be for JVMs spawned by Gradle as well.  This
        # happens during the maven deployment generating the GeckoView
        # documents; this works around "error: unmappable character
        # for encoding ASCII" in exoplayer2.  See
        # https://discuss.gradle.org/t/unmappable-character-for-encoding-ascii-when-building-a-utf-8-project/10692/11
        # and especially https://stackoverflow.com/a/21755671.

        return self.run_process([self.substs['GRADLE']] + gradle_flags + ['--console=plain'] + args,
            append_env={
                'GRADLE_OPTS': '-Dfile.encoding=utf-8',
                'JAVA_HOME': java_home,
                'JAVA_TOOL_OPTIONS': '-Dfile.encoding=utf-8',
            },
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

    @Command('gradle-install', category='devenv',
        conditions=[REMOVED])
    def gradle_install(self):
        pass


@CommandProvider
class AndroidEmulatorCommands(MachCommandBase):
    """
       Run the Android emulator with one of the AVDs used in the Mozilla
       automated test environment. If necessary, the AVD is fetched from
       the tooltool server and installed.
    """
    @Command('android-emulator', category='devenv',
        conditions=[],
        description='Run the Android emulator with an AVD from test automation.')
    @CommandArgument('--version', metavar='VERSION', choices=['4.3', '6.0', '7.0', 'x86', 'x86-6.0', 'x86-7.0'],
        help='Specify Android version to run in emulator. One of "4.3", "6.0", "7.0", "x86", "x86-6.0", or "x86-7.0".',
        default='4.3')
    @CommandArgument('--wait', action='store_true',
        help='Wait for emulator to be closed.')
    @CommandArgument('--force-update', action='store_true',
        help='Update AVD definition even when AVD is already installed.')
    @CommandArgument('--verbose', action='store_true',
        help='Log informative status messages.')
    def emulator(self, version, wait=False, force_update=False, verbose=False):
        from mozrunner.devices.android_device import AndroidEmulator

        emulator = AndroidEmulator(version, verbose, substs=self.substs, device_serial='emulator-5554')
        if emulator.is_running():
            # It is possible to run multiple emulators simultaneously, but:
            #  - if more than one emulator is using the same avd, errors may
            #    occur due to locked resources;
            #  - additional parameters must be specified when running tests,
            #    to select a specific device.
            # To avoid these complications, allow just one emulator at a time.
            self.log(logging.ERROR, "emulator", {},
                     "An Android emulator is already running.\n"
                     "Close the existing emulator and re-run this command.")
            return 1

        if not emulator.is_available():
            self.log(logging.WARN, "emulator", {},
                     "Emulator binary not found.\n"
                     "Install the Android SDK and make sure 'emulator' is in your PATH.")
            return 2

        if not emulator.check_avd(force_update):
            self.log(logging.INFO, "emulator", {},
                     "Fetching and installing AVD. This may take a few minutes...")
            emulator.update_avd(force_update)

        self.log(logging.INFO, "emulator", {},
                 "Starting Android emulator running %s..." %
                 emulator.get_avd_description())
        emulator.start()
        if emulator.wait_for_start():
            self.log(logging.INFO, "emulator", {},
                     "Android emulator is running.")
        else:
            # This is unusual but the emulator may still function.
            self.log(logging.WARN, "emulator", {},
                     "Unable to verify that emulator is running.")

        if conditions.is_android(self):
            self.log(logging.INFO, "emulator", {},
                     "Use 'mach install' to install or update Firefox on your emulator.")
        else:
            self.log(logging.WARN, "emulator", {},
                     "No Firefox for Android build detected.\n"
                     "Switch to a Firefox for Android build context or use 'mach bootstrap'\n"
                     "to setup an Android build environment.")

        if wait:
            self.log(logging.INFO, "emulator", {},
                     "Waiting for Android emulator to close...")
            rc = emulator.wait()
            if rc is not None:
                self.log(logging.INFO, "emulator", {},
                         "Android emulator completed with return code %d." % rc)
            else:
                self.log(logging.WARN, "emulator", {},
                         "Unable to retrieve Android emulator return code.")
        return 0


@CommandProvider
class AutophoneCommands(MachCommandBase):
    """
       Run autophone, https://wiki.mozilla.org/Auto-tools/Projects/Autophone.

       If necessary, autophone is cloned from github, installed, and configured.
    """
    @Command('autophone', category='devenv',
        conditions=[],
        description='Run autophone.')
    @CommandArgument('--clean', action='store_true',
        help='Delete an existing autophone installation.')
    @CommandArgument('--verbose', action='store_true',
        help='Log informative status messages.')
    def autophone(self, clean=False, verbose=False):
        import platform
        from mozrunner.devices.autophone import AutophoneRunner

        if platform.system() == "Windows":
            # Autophone is normally run on Linux or OSX.
            self.log(logging.ERROR, "autophone", {},
                "This mach command is not supported on Windows!")
            return -1

        runner = AutophoneRunner(self, verbose)
        runner.load_config()
        if clean:
            runner.reset_to_clean()
            return 0
        if not runner.setup_directory():
            return 1
        if not runner.install_requirements():
            runner.save_config()
            return 2
        if not runner.configure():
            runner.save_config()
            return 3
        runner.save_config()
        runner.launch_autophone()
        runner.command_prompts()
        return 0
