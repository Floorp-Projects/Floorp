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

from mozbuild.util import (
    FileAvoidWrite,
)

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

SUCCESS = '''
You should be ready to build with Gradle and import into IntelliJ!  Test with

    ./mach gradle build

and in IntelliJ select File > Import project... and choose

    {topobjdir}/mobile/android/gradle
'''


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

        code = self.gradle_install(quiet=True)
        if code:
            return code

        return self.run_process(['./gradlew'] + args,
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topobjdir, 'mobile', 'android', 'gradle'))

    @Command('gradle-install', category='devenv',
        description='Install gradle environment.',
        conditions=[conditions.is_android])
    def gradle_install(self, quiet=False):
        import mozpack.manifests
        m = mozpack.manifests.InstallManifest()

        def srcdir(dst, src):
            m.add_symlink(os.path.join(self.topsrcdir, src), dst)

        srcdir('build.gradle', 'mobile/android/gradle/build.gradle')
        srcdir('settings.gradle', 'mobile/android/gradle/settings.gradle')

        m.add_pattern_copy(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradle/wrapper'), '**', 'gradle/wrapper')
        m.add_copy(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradlew'), 'gradlew')

        defines = {
            'topsrcdir': self.topsrcdir,
            'topobjdir': self.topobjdir,
            'ANDROID_SDK_ROOT': self.substs['ANDROID_SDK_ROOT'],
        }
        m.add_preprocess(os.path.join(self.topsrcdir, 'mobile/android/gradle/gradle.properties.in'),
            'gradle.properties',
            defines=defines,
            deps=os.path.join(self.topobjdir, 'mobile/android/gradle/.deps/gradle.properties.pp'))
        m.add_preprocess(os.path.join(self.topsrcdir, 'mobile/android/gradle/local.properties.in'),
            'local.properties',
            defines=defines,
            deps=os.path.join(self.topobjdir, 'mobile/android/gradle/.deps/local.properties.pp'))

        srcdir('thirdparty/build.gradle', 'mobile/android/gradle/thirdparty/build.gradle')
        srcdir('thirdparty/src/main/AndroidManifest.xml', 'mobile/android/gradle/thirdparty/AndroidManifest.xml')
        srcdir('thirdparty/src/main/java', 'mobile/android/thirdparty')

        srcdir('omnijar/build.gradle', 'mobile/android/gradle/omnijar/build.gradle')
        srcdir('omnijar/src/main/java/locales', 'mobile/android/locales')
        srcdir('omnijar/src/main/java/chrome', 'mobile/android/chrome')
        srcdir('omnijar/src/main/java/components', 'mobile/android/components')
        srcdir('omnijar/src/main/java/modules', 'mobile/android/modules')
        srcdir('omnijar/src/main/java/themes', 'mobile/android/themes')

        srcdir('app/build.gradle', 'mobile/android/gradle/app/build.gradle')
        srcdir('app/src/androidTest/res', 'mobile/android/tests/browser/robocop/res')
        srcdir('app/src/androidTest/assets', 'mobile/android/tests/browser/robocop/assets')
        # Test code.
        srcdir('app/src/robocop', 'mobile/android/tests/browser/robocop/src')
        srcdir('app/src/background', 'mobile/android/tests/background/junit3/src')
        srcdir('app/src/browser', 'mobile/android/tests/browser/junit3/src')
        srcdir('app/src/javaaddons', 'mobile/android/tests/javaaddons/src')

        srcdir('base/build.gradle', 'mobile/android/gradle/base/build.gradle')
        srcdir('base/lint.xml', 'mobile/android/gradle/base/lint.xml')
        srcdir('base/src/main/AndroidManifest.xml', 'mobile/android/gradle/base/AndroidManifest.xml')
        srcdir('base/src/main/java/org/mozilla/gecko', 'mobile/android/base')
        srcdir('base/src/main/java/org/mozilla/mozstumbler', 'mobile/android/stumbler/java/org/mozilla/mozstumbler')
        srcdir('base/src/main/java/org/mozilla/search', 'mobile/android/search/java/org/mozilla/search')
        srcdir('base/src/main/java/org/mozilla/javaaddons', 'mobile/android/javaaddons/java/org/mozilla/javaaddons')
        srcdir('base/src/webrtc_audio_device/java', 'media/webrtc/trunk/webrtc/modules/audio_device/android/java/src')
        srcdir('base/src/webrtc_video_capture/java', 'media/webrtc/trunk/webrtc/modules/video_capture/android/java/src')
        srcdir('base/src/webrtc_video_render/java', 'media/webrtc/trunk/webrtc/modules/video_render/android/java/src')
        srcdir('base/src/main/res', 'mobile/android/base/resources')
        srcdir('base/src/main/assets', 'mobile/android/app/assets')
        srcdir('base/src/crashreporter/res', 'mobile/android/base/crashreporter/res')
        srcdir('base/src/branding/res', os.path.join(self.substs['MOZ_BRANDING_DIRECTORY'], 'res'))
        # JUnit 4 test code.
        srcdir('base/src/background_junit4', 'mobile/android/tests/background/junit4/src')
        srcdir('base/resources/background_junit4', 'mobile/android/tests/background/junit4/resources')

        manifest_path = os.path.join(self.topobjdir, 'mobile', 'android', 'gradle.manifest')
        with FileAvoidWrite(manifest_path) as f:
            m.write(fileobj=f)

        self.virtualenv_manager.ensure()
        code = self.run_process([
                self.virtualenv_manager.python_path,
                os.path.join(self.topsrcdir, 'python/mozbuild/mozbuild/action/process_install_manifest.py'),
                '--no-remove',
                '--no-remove-all-directory-symlinks',
                '--no-remove-empty-directories',
                os.path.join(self.topobjdir, 'mobile', 'android', 'gradle'),
                manifest_path],
            pass_thru=True, # Allow user to run gradle interactively.
            ensure_exit_code=False, # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir, 'mobile', 'android'))

        if not quiet:
            if not code:
                print(SUCCESS.format(topobjdir=self.topobjdir))

        return code


class ArtifactSubCommand(SubCommand):
    def __init__(self, *args, **kwargs):
        SubCommand.__init__(self, *args, **kwargs)

    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        args = [
            CommandArgument('--tree', metavar='TREE', type=str,
                help='Firefox tree.'),
            CommandArgument('--job', metavar='JOB', choices=['android-api-11', 'android-x86'],
                help='Build job.'),
            CommandArgument('--verbose', '-v', action='store_true',
                help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


@CommandProvider
class PackageFrontend(MachCommandBase):
    """Fetch and install binary artifacts from Mozilla automation."""

    @Command('artifact', category='post-build',
        description='Use pre-built artifacts to build Fennec.',
        conditions=[
            conditions.is_android,  # mobile/android only for now.
            conditions.is_hg,  # mercurial only for now.
        ])
    def artifact(self):
        '''Download, cache, and install pre-built binary artifacts to build Fennec.

        Invoke |mach artifact| before each |mach package| to freshen your installed
        binary libraries.  That is, package using

        mach artifact install && mach package

        to download, cache, and install binary artifacts from Mozilla automation,
        replacing whatever may be in your object directory.  Use |mach artifact last|
        to see what binary artifacts were last used.

        Never build libxul again!
        '''
        pass

    def _set_log_level(self, verbose):
        self.log_manager.terminal_handler.setLevel(logging.INFO if not verbose else logging.DEBUG)

    def _make_artifacts(self, tree=None, job=None):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('pylru==1.0.9')
        self.virtualenv_manager.install_pip_package('taskcluster==0.0.16')
        self.virtualenv_manager.install_pip_package('mozregression==1.0.2')

        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        import which
        hg = which.which('hg')

        # Absolutely must come after the virtualenv is populated!
        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, job, log=self.log, cache_dir=cache_dir, hg=hg)
        return artifacts

    def _compute_defaults(self, tree=None, job=None):
        # Firefox front-end developers mostly use fx-team.  Post auto-land, make this central.
        tree = tree or 'fx-team'
        if job:
            return (tree, job)
        if self.substs['ANDROID_CPU_ARCH'] == 'x86':
            return (tree, 'android-x86')
        return (tree, 'android-api-11')

    @ArtifactSubCommand('artifact', 'install',
        'Install a good pre-built artifact.')
    @CommandArgument('source', metavar='SRC', nargs='?', type=str,
        help='Where to fetch and install artifacts from.  Can be omitted, in '
            'which case the current hg repository is inspected; an hg revision; '
            'a remote URL; or a local file.',
        default=None)
    def artifact_install(self, source=None, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        tree, job = self._compute_defaults(tree, job)
        artifacts = self._make_artifacts(tree=tree, job=job)
        return artifacts.install_from(source, self.distdir)

    @ArtifactSubCommand('artifact', 'last',
        'Print the last pre-built artifact installed.')
    def artifact_print_last(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        tree, job = self._compute_defaults(tree, job)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_last()
        return 0

    @ArtifactSubCommand('artifact', 'print-cache',
        'Print local artifact cache for debugging.')
    def artifact_print_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        tree, job = self._compute_defaults(tree, job)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.print_cache()
        return 0

    @ArtifactSubCommand('artifact', 'clear-cache',
        'Delete local artifacts and reset local artifact cache.')
    def artifact_clear_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        tree, job = self._compute_defaults(tree, job)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0

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
    @CommandArgument('--version', metavar='VERSION', choices=['2.3', '4.3', 'x86'],
        help='Specify Android version to run in emulator. One of "2.3", "4.3", or "x86".',
        default='4.3')
    @CommandArgument('--wait', action='store_true',
        help='Wait for emulator to be closed.')
    @CommandArgument('--force-update', action='store_true',
        help='Update AVD definition even when AVD is already installed.')
    @CommandArgument('--verbose', action='store_true',
        help='Log informative status messages.')
    def emulator(self, version, wait=False, force_update=False, verbose=False):
        from mozrunner.devices.android_device import AndroidEmulator

        emulator = AndroidEmulator(version, verbose, substs=self.substs)
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
