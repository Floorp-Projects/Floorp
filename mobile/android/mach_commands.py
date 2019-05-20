# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import json

from zipfile import ZipFile

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
    """Command no longer exists! Use the Gradle configuration rooted in the top source directory
    instead.

    See https://developer.mozilla.org/en-US/docs/Simple_Firefox_for_Android_build#Developing_Firefox_for_Android_in_Android_Studio_or_IDEA_IntelliJ.  # NOQA: E501
    """
    return False


@CommandProvider
class MachCommands(MachCommandBase):
    def _root_url(self, artifactdir=None, objdir=None):
        """Generate a publicly-accessible URL for the tasks's artifacts, or an objdir path"""
        if 'TASK_ID' in os.environ and 'RUN_ID' in os.environ:
            import taskcluster_urls
            from taskgraph.util.taskcluster import get_root_url
            return taskcluster_urls.api(
                get_root_url(False), 'queue', 'v1', 'task/{}/runs/{}/artifacts/{}'.format(
                    os.environ['TASK_ID'], os.environ['RUN_ID'], artifactdir))
        else:
            return os.path.join(self.topobjdir, objdir)

    @Command('android', category='devenv',
             description='Run Android-specific commands.',
             conditions=[conditions.is_android])
    def android(self):
        pass

    @SubCommand('android', 'assemble-app',
                """Assemble Firefox for Android.
        See http://firefox-source-docs.mozilla.org/build/buildsystem/toolchains.html#firefox-for-android-with-gradle""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_assemble_app(self, args):
        ret = self.gradle(self.substs['GRADLE_ANDROID_APP_TASKS'] +
                          ['-x', 'lint'] + args, verbose=True)

        return ret

    @SubCommand('android', 'generate-sdk-bindings',
                """Generate SDK bindings used when building GeckoView.""")
    @CommandArgument('inputs', nargs='+', help='config files, '
                     'like [/path/to/ClassName-classes.txt]+')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_generate_sdk_bindings(self, inputs, args):
        import itertools

        def stem(input):
            # Turn "/path/to/ClassName-classes.txt" into "ClassName".
            return os.path.basename(input).rsplit('-classes.txt', 1)[0]

        bindings_inputs = list(itertools.chain(*((input, stem(input)) for input in inputs)))
        bindings_args = '-Pgenerate_sdk_bindings_args={}'.format(':'.join(bindings_inputs))

        ret = self.gradle(
            self.substs['GRADLE_ANDROID_GENERATE_SDK_BINDINGS_TASKS'] + [bindings_args] + args,
            verbose=True)

        return ret

    @SubCommand('android', 'generate-generated-jni-wrappers',
                """Generate GeckoView JNI wrappers used when building GeckoView.""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_generate_generated_jni_wrappers(self, args):
        ret = self.gradle(
            self.substs['GRADLE_ANDROID_GENERATE_GENERATED_JNI_WRAPPERS_TASKS'] + args,
            verbose=True)

        return ret

    @SubCommand('android', 'generate-fennec-jni-wrappers',
                """Generate Fennec-specific JNI wrappers used when building
                Firefox for Android.""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_generate_fennec_jni_wrappers(self, args):
        ret = self.gradle(
            self.substs['GRADLE_ANDROID_GENERATE_FENNEC_JNI_WRAPPERS_TASKS'] + args, verbose=True)

        return ret

    @SubCommand('android', 'api-lint',
                """Runs apilint against GeckoView.""")
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_api_lint(self, args):
        ret = self.gradle(self.substs['GRADLE_ANDROID_API_LINT_TASKS'] + args, verbose=True)
        folder = self.substs['GRADLE_ANDROID_GECKOVIEW_APILINT_FOLDER']

        with open(os.path.join(
                self.topobjdir,
                '{}/apilint-result.json'.format(folder))) as f:
            result = json.load(f)

            print('SUITE-START | android-api-lint')
            for r in result['compat_failures'] + result['failures']:
                print ('TEST-UNEXPECTED-FAIL | {} | {}'.format(r['detail'], r['msg']))
            for r in result['api_changes']:
                print ('TEST-UNEXPECTED-FAIL | {} | Unexpected api change'.format(r))
            print('SUITE-END | android-api-lint')

        return ret

    @SubCommand('android', 'test',
                """Run Android local unit tests.
                See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-test""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_test(self, args):
        ret = self.gradle(self.substs['GRADLE_ANDROID_TEST_TASKS'] +
                          args, verbose=True)

        ret |= self._parse_android_test_results('public/app/unittest',
                                                'gradle/build/mobile/android/app',
                                                (self.substs['GRADLE_ANDROID_APP_VARIANT_NAME'],))

        ret |= self._parse_android_test_results('public/geckoview/unittest',
                                                'gradle/build/mobile/android/geckoview',
                                                (self.substs['GRADLE_ANDROID_GECKOVIEW_VARIANT_NAME'],))  # NOQA: E501

        return ret

    def _parse_android_test_results(self, artifactdir, gradledir, variants):
        # Unit tests produce both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import itertools
        import xml.etree.ElementTree as ET

        from mozpack.files import (
            FileFinder,
        )

        ret = 0
        found_reports = False

        root_url = self._root_url(
            artifactdir=artifactdir,
            objdir=gradledir + '/reports/tests')

        def capitalize(s):
            # Can't use str.capitalize because it lower cases trailing letters.
            return (s[0].upper() + s[1:]) if s else ''

        for variant in variants:
            report = 'test{}UnitTest'.format(capitalize(variant))
            finder = FileFinder(os.path.join(self.topobjdir, gradledir + '/test-results/', report))
            for p, _ in finder.find('TEST-*.xml'):
                found_reports = True
                f = open(os.path.join(finder.base, p), 'rt')
                tree = ET.parse(f)
                root = tree.getroot()

                # Log reports for Tree Herder "Job Details".
                print('TinderboxPrint: report<br/><a href="{}/{}/index.html">HTML {} report</a>, visit "Inspect Task" link for details'.format(root_url, report, report))  # NOQA: E501

                # And make the report display as soon as possible.
                failed = root.findall('testcase/error') or root.findall('testcase/failure')
                if failed:
                    print(
                        'TEST-UNEXPECTED-FAIL | android-test | There were failing tests. See the reports at: {}/{}/index.html'.format(root_url, report))  # NOQA: E501

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

        if not found_reports:
            print('TEST-UNEXPECTED-FAIL | android-test | No reports found under {}'.format(gradledir))  # NOQA: E501
            return 1

        return ret

    @SubCommand('android', 'lint',
                """Run Android lint.
                See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-lint""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_lint(self, args):
        ret = self.gradle(self.substs['GRADLE_ANDROID_LINT_TASKS'] +
                          args, verbose=True)

        # Android Lint produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        root_url = self._root_url(
            artifactdir='public/android/lint',
            objdir='gradle/build/mobile/android/app/reports')

        reports = (self.substs['GRADLE_ANDROID_APP_VARIANT_NAME'],)
        for report in reports:
            f = open(os.path.join(
                self.topobjdir,
                'gradle/build/mobile/android/app/reports/lint-results-{}.xml'.format(report)),
                     'rt')
            tree = ET.parse(f)
            root = tree.getroot()

            # Log reports for Tree Herder "Job Details".
            html_report_url = '{}/lint-results-{}.html'.format(root_url, report)
            xml_report_url = '{}/lint-results-{}.xml'.format(root_url, report)
            print('TinderboxPrint: report<br/><a href="{}">HTML {} report</a>, visit "Inspect Task" link for details'.format(html_report_url, report))  # NOQA: E501
            print('TinderboxPrint: report<br/><a href="{}">XML {} report</a>, visit "Inspect Task" link for details'.format(xml_report_url, report))  # NOQA: E501

            # And make the report display as soon as possible.
            if root.findall("issue[@severity='Error']"):
                print('TEST-UNEXPECTED-FAIL | android-lint | Lint found errors in the project; aborting build. See the report at: {}'.format(html_report_url))  # NOQA: E501

            print('SUITE-START | android-lint | {}'.format(report))
            for issue in root.findall("issue[@severity='Error']"):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <issue> XML
                # tag.
                for line in ET.tostring(issue).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}'.format(line))
                ret |= 1
            print('SUITE-END | android-lint | {}'.format(report))

        return ret

    def _parse_checkstyle_output(self, output_path):
        ret = 0
        # Checkstyle produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        output_absolute_path = os.path.join(self.topobjdir, output_path)
        f = open(output_absolute_path, 'rt')
        tree = ET.parse(f)
        root = tree.getroot()

        # Now the reports, linkified.
        report_xml = self._root_url(
            artifactdir='public/android/checkstyle',
            objdir=output_absolute_path)
        report_html = self._root_url(
            artifactdir='public/android/checkstyle',
            objdir=os.path.splitext(output_absolute_path)[0] + '.html')

        # And make the report display as soon as possible.
        if root.findall('file/error'):
            ret |= 1

        if ret:
            # Log reports for Tree Herder "Job Details".
            print('TinderboxPrint: report<br/><a href="{}">HTML checkstyle report</a>, visit "Inspect Task" link for details'.format(report_xml))  # NOQA: E501
            print('TinderboxPrint: report<br/><a href="{}">XML checkstyle report</a>, visit "Inspect Task" link for details'.format(report_html))  # NOQA: E501

            print('TEST-UNEXPECTED-FAIL | android-checkstyle | Checkstyle rule violations were found. See the report at: {}'.format(report_html))  # NOQA: E501

        for file in root.findall('file'):
            name = file.get('name')

            error_count = 0
            for error in file.findall('error'):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <error> XML
                # tag.
                print('TEST-UNEXPECTED-FAIL | {}'.format(name))
                for line in ET.tostring(error).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}'.format(line))
                error_count += 1

        return ret

    @SubCommand('android', 'checkstyle',
                """Run Android checkstyle.
                See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-checkstyle""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_checkstyle(self, args):
        ret = self.gradle(self.substs['GRADLE_ANDROID_CHECKSTYLE_TASKS'] +
                          args, verbose=True)
        print('SUITE-START | android-checkstyle')
        for filePath in self.substs['GRADLE_ANDROID_CHECKSTYLE_OUTPUT_FILES']:
            ret |= self._parse_checkstyle_output(filePath)
        print('SUITE-END | android-checkstyle')

        return ret

    @SubCommand('android', 'findbugs',
                """Run Android findbugs.
                See https://developer.mozilla.org/en-US/docs/Mozilla/Android-specific_test_suites#android-findbugs""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_findbugs(self, dryrun=False, args=[]):
        ret = self.gradle(self.substs['GRADLE_ANDROID_FINDBUGS_TASKS'] +
                          args, verbose=True)

        # Findbug produces both HTML and XML reports.  Visit the
        # XML report(s) to report errors and link to the HTML
        # report(s) for human consumption.
        import xml.etree.ElementTree as ET

        root_url = self._root_url(
            artifactdir='public/android/findbugs',
            objdir='gradle/build/mobile/android/app/reports/findbugs')

        reports = (self.substs['GRADLE_ANDROID_APP_VARIANT_NAME'],)
        for report in reports:
            try:
                f = open(os.path.join(
                    self.topobjdir, 'gradle/build/mobile/android/app/reports/findbugs',
                    'findbugs-{}-output.xml'.format(report)),
                         'rt')
            except IOError:
                continue

            tree = ET.parse(f)
            root = tree.getroot()

            # Log reports for Tree Herder "Job Details".
            html_report_url = '{}/findbugs-{}-output.html'.format(root_url, report)
            xml_report_url = '{}/findbugs-{}-output.xml'.format(root_url, report)
            print('TinderboxPrint: report<br/><a href="{}">HTML {} report</a>, visit "Inspect Task" link for details'.format(html_report_url, report))  # NOQA: E501
            print('TinderboxPrint: report<br/><a href="{}">XML {} report</a>, visit "Inspect Task" link for details'.format(xml_report_url, report))  # NOQA: E501

            # And make the report display as soon as possible.
            if root.findall("./BugInstance"):
                print('TEST-UNEXPECTED-FAIL | android-findbugs | Findbugs found issues in the project. See the report at: {}'.format(html_report_url))  # NOQA: E501

            print('SUITE-START | android-findbugs | {}'.format(report))
            for error in root.findall('./BugInstance'):
                # There's no particular advantage to formatting the
                # error, so for now let's just output the <error> XML
                # tag.
                print('TEST-UNEXPECTED-FAIL | {}:{} | {}'.format(report,
                                                                 error.get('type'),
                                                                 error.find('Class')
                                                                 .get('classname')))
                for line in ET.tostring(error).strip().splitlines():
                    print('TEST-UNEXPECTED-FAIL | {}:{} | {}'.format(report,
                                                                     error.get('type'),
                                                                     line))
                ret |= 1
            print('SUITE-END | android-findbugs | {}'.format(report))

        return ret

    @SubCommand('android', 'gradle-dependencies',
                """Collect Android Gradle dependencies.
        See http://firefox-source-docs.mozilla.org/build/buildsystem/toolchains.html#firefox-for-android-with-gradle""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_gradle_dependencies(self, args):
        # We don't want to gate producing dependency archives on clean
        # lint or checkstyle, particularly because toolchain versions
        # can change the outputs for those processes.
        self.gradle(self.substs['GRADLE_ANDROID_DEPENDENCIES_TASKS'] +
                    ["--continue"] + args, verbose=True)

        return 0

    @SubCommand('android', 'archive-geckoview',
                """Create GeckoView archives.
        See http://firefox-source-docs.mozilla.org/build/buildsystem/toolchains.html#firefox-for-android-with-gradle""")  # NOQA: E501
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_archive_geckoview(self, args):
        ret = self.gradle(
            self.substs['GRADLE_ANDROID_ARCHIVE_GECKOVIEW_TASKS'] + args,
            verbose=True)

        if ret != 0:
            return ret

        # The zip archive is passed along in CI to ship geckoview onto a maven repo
        _craft_maven_zip_archive(self.topobjdir)

        return 0

    @SubCommand('android', 'build-geckoview_example',
                """Build geckoview_example """)
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_build_geckoview_example(self, args):
        self.gradle(self.substs['GRADLE_ANDROID_BUILD_GECKOVIEW_EXAMPLE_TASKS'] + args,
                    verbose=True)

        print('Execute `mach android install-geckoview_example` '
              'to push the geckoview_example and test APKs to a device.')

        return 0

    @SubCommand('android', 'install-geckoview_example',
                """Install geckoview_example """)
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def android_install_geckoview_example(self, args):
        self.gradle(self.substs['GRADLE_ANDROID_INSTALL_GECKOVIEW_EXAMPLE_TASKS'] + args,
                    verbose=True)

        print('Execute `mach android build-geckoview_example` '
              'to just build the geckoview_example and test APKs.')

        return 0

    @SubCommand('android', 'geckoview-docs',
                """Create GeckoView javadoc and optionally upload to Github""")
    @CommandArgument('--archive', action='store_true',
                     help='Generate a javadoc archive.')
    @CommandArgument('--upload', metavar='USER/REPO',
                     help='Upload generated javadoc to Github, '
                     'using the specified USER/REPO.')
    @CommandArgument('--upload-branch', metavar='BRANCH[/PATH]',
                     default='gh-pages/javadoc',
                     help='Use the specified branch/path for commits.')
    @CommandArgument('--upload-message', metavar='MSG',
                     default='GeckoView docs upload',
                     help='Use the specified message for commits.')
    def android_geckoview_docs(self, archive, upload, upload_branch,
                               upload_message):

        tasks = (self.substs['GRADLE_ANDROID_GECKOVIEW_DOCS_ARCHIVE_TASKS'] if archive or upload
                 else self.substs['GRADLE_ANDROID_GECKOVIEW_DOCS_TASKS'])

        ret = self.gradle(tasks, verbose=True)
        if ret or not upload:
            return ret

        # Upload to Github.
        fmt = {
            'level': os.environ.get('MOZ_SCM_LEVEL', '0'),
            'project': os.environ.get('MH_BRANCH', 'unknown'),
            'revision': os.environ.get('GECKO_HEAD_REV', 'tip'),
        }
        env = {}

        # In order to push to GitHub from TaskCluster, we store a private key
        # in the TaskCluster secrets store in the format {"content": "<KEY>"},
        # and the corresponding public key as a writable deploy key for the
        # destination repo on GitHub.
        secret = os.environ.get('GECKOVIEW_DOCS_UPLOAD_SECRET', '').format(**fmt)
        if secret:
            # Set up a private key from the secrets store if applicable.
            import requests
            req = requests.get('http://taskcluster/secrets/v1/secret/' + secret)
            req.raise_for_status()

            keyfile = mozpath.abspath('gv-docs-upload-key')
            with open(keyfile, 'w') as f:
                os.chmod(keyfile, 0o600)
                f.write(req.json()['secret']['content'])

            # Turn off strict host key checking so ssh does not complain about
            # unknown github.com host. We're not pushing anything sensitive, so
            # it's okay to not check GitHub's host keys.
            env['GIT_SSH_COMMAND'] = 'ssh -i "%s" -o StrictHostKeyChecking=no' % keyfile

        # Clone remote repo.
        branch, _, branch_path = upload_branch.partition('/')
        repo_url = 'git@github.com:%s.git' % upload
        repo_path = mozpath.abspath('gv-docs-repo')
        self.run_process(['git', 'clone', '--branch', branch, '--depth', '1',
                          repo_url, repo_path], append_env=env, pass_thru=True)
        env['GIT_DIR'] = mozpath.join(repo_path, '.git')
        env['GIT_WORK_TREE'] = repo_path
        env['GIT_AUTHOR_NAME'] = env['GIT_COMMITTER_NAME'] = 'GeckoView Docs Bot'
        env['GIT_AUTHOR_EMAIL'] = env['GIT_COMMITTER_EMAIL'] = 'nobody@mozilla.com'

        # Extract new javadoc to specified directory inside repo.
        import mozfile
        src_tar = mozpath.join(self.topobjdir, 'gradle', 'build', 'mobile', 'android',
                               'geckoview', 'libs', 'geckoview-javadoc.jar')
        dst_path = mozpath.join(repo_path, branch_path.format(**fmt))
        mozfile.remove(dst_path)
        mozfile.extract_zip(src_tar, dst_path)

        # Commit and push.
        self.run_process(['git', 'add', '--all'], append_env=env, pass_thru=True)
        if self.run_process(['git', 'diff', '--cached', '--quiet'],
                            append_env=env, pass_thru=True, ensure_exit_code=False) != 0:
            # We have something to commit.
            self.run_process(['git', 'commit',
                              '--message', upload_message.format(**fmt)],
                             append_env=env, pass_thru=True)
            self.run_process(['git', 'push', 'origin', 'gh-pages'],
                             append_env=env, pass_thru=True)

        mozfile.remove(repo_path)
        if secret:
            mozfile.remove(keyfile)
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

        gradle_flags = self.substs.get('GRADLE_FLAGS', '') or \
            os.environ.get('GRADLE_FLAGS', '')
        gradle_flags = shell_split(gradle_flags)

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
        # https://discuss.gradle.org/t/unmappable-character-for-encoding-ascii-when-building-a-utf-8-project/10692/11  # NOQA: E501
        # and especially https://stackoverflow.com/a/21755671.

        if self.substs.get('MOZ_AUTOMATION'):
            gradle_flags += ['--console=plain']

        return self.run_process(
            [self.substs['GRADLE']] + gradle_flags + args,
            append_env={
                'GRADLE_OPTS': '-Dfile.encoding=utf-8',
                'JAVA_HOME': java_home,
                'JAVA_TOOL_OPTIONS': '-Dfile.encoding=utf-8',
            },
            pass_thru=True,  # Allow user to run gradle interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

    @Command('gradle-install', category='devenv',
             conditions=[REMOVED])
    def gradle_install(self):
        pass

    @Command('install-android', category='post-build',
             conditional_name='install',
             conditions=[conditions.is_android],
             description='Install an Android package on a device or an emulator.')
    @CommandArgument('--verbose', '-v', action='store_true',
                     help='Print verbose output when installing.')
    def install(self, verbose=False):
        from mozrunner.devices.android_device import verify_android_device
        verify_android_device(self, verbose=verbose)

        ret = self._run_make(directory='.', target='install', ensure_exit_code=False)
        if ret == 0:
            self.notify('Install complete')
        return ret

    @Command('run-android', category='post-build',
             conditional_name='run',
             conditions=[conditions.is_android],
             description='Run Fennec on an Android device or an emulator.')
    @CommandArgument('--url', help='URL to open',
                     default=None)
    @CommandArgument('--no-install', help='Do not try to install application on device before ' +
                     'running (default: False)',
                     action='store_true',
                     default=False)
    @CommandArgument('--no-wait', help='Do not wait for application to start before returning ' +
                     '(default: False)',
                     action='store_true',
                     default=False)
    @CommandArgument('--fail-if-running', help='Fail if application is already running ' +
                     '(default: False)',
                     action='store_true',
                     default=False)
    def run(self, url=None, no_install=None, no_wait=None, fail_if_running=None):
        from mozrunner.devices.android_device import verify_android_device, run_firefox_for_android

        verify_android_device(self, install=not no_install)
        return run_firefox_for_android(self,
                                       [],
                                       url=url,
                                       wait=not no_wait,
                                       fail_if_running=fail_if_running)


def _get_maven_archive_abs_and_relative_paths(maven_folder):
    for subdir, _, files in os.walk(maven_folder):
        for file in files:
            full_path = os.path.join(subdir, file)
            relative_path = os.path.relpath(full_path, maven_folder)

            # maven-metadata is intended to be generated on the real maven server
            if 'maven-metadata.xml' not in relative_path:
                yield full_path, relative_path


def _craft_maven_zip_archive(topobjdir):
    geckoview_folder = os.path.join(topobjdir, 'gradle/build/mobile/android/geckoview')
    maven_folder = os.path.join(geckoview_folder, 'maven')

    with ZipFile(os.path.join(geckoview_folder, 'target.maven.zip'), 'w') as target_zip:
        for abs, rel in _get_maven_archive_abs_and_relative_paths(maven_folder):
            target_zip.write(abs, arcname=rel)


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
    @CommandArgument('--version', metavar='VERSION',
                     choices=['4.3', 'x86', 'x86-7.0'],
                     help='Specify Android version to run in emulator. '
                     'One of "4.3", "x86", or "x86-7.0".')
    @CommandArgument('--wait', action='store_true',
                     help='Wait for emulator to be closed.')
    @CommandArgument('--force-update', action='store_true',
                     help='Update AVD definition even when AVD is already installed.')
    @CommandArgument('--verbose', action='store_true',
                     help='Log informative status messages.')
    def emulator(self, version, wait=False, force_update=False, verbose=False):
        from mozrunner.devices.android_device import AndroidEmulator

        emulator = AndroidEmulator(version, verbose, substs=self.substs,
                                   device_serial='emulator-5554')
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
