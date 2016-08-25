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
        description='Run the Android package manager tool.',
        conditions=[conditions.is_android])
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android(self, args):
        # Avoid logging the command
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)

        return self.run_process(
            [os.path.join(self.substs['ANDROID_TOOLS'], 'android')] + args,
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

    @Command('gradle', category='devenv',
        description='Run gradle.',
        conditions=[conditions.is_android])
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def gradle(self, args):
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
        return self.run_process([self.substs['GRADLE']] + gradle_flags + args,
            append_env={
                'GRADLE_OPTS': '-Dfile.encoding=utf-8',
                'JAVA_HOME': java_home,
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
    @CommandArgument('--version', metavar='VERSION', choices=['4.3', '6.0', 'x86'],
        help='Specify Android version to run in emulator. One of "4.3", "6.0", or "x86".',
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
