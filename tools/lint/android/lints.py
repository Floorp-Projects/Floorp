# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import itertools
import json
import os
import re
import six
import subprocess
import sys

import xml.etree.ElementTree as ET

from mozpack.files import (
    FileFinder,
)
import mozpack.path as mozpath
from mozlint import result


# The Gradle target invocations are serialized with a simple locking file scheme.  It's fine for
# them to take a while, since the first will compile all the Java, etc, and then perform
# potentially expensive static analyses.
GRADLE_LOCK_MAX_WAIT_SECONDS = 20 * 60


def setup(root, **setupargs):
    if setupargs.get('substs', {}).get('MOZ_BUILD_APP') != 'mobile/android':
        return 1

    if 'topobjdir' not in setupargs:
        print('Skipping {}: a configured Android build is required!'.format(setupargs['name']))
        return 1

    return 0


def gradle(topsrcdir=None, topobjdir=None, tasks=[], extra_args=[], verbose=True):
    sys.path.insert(0, os.path.join(topsrcdir, 'mobile', 'android'))
    from gradle import gradle_lock

    with gradle_lock(topobjdir, max_wait_seconds=GRADLE_LOCK_MAX_WAIT_SECONDS), \
            open(os.devnull, 'wb') as devnull:
        cmd_args = [sys.executable, os.path.join(topsrcdir, 'mach'),
                    'gradle', '--verbose', '--'] + \
            tasks + \
            extra_args

        if verbose:
            print(' '.join(six.moves.shlex_quote(arg) for arg in cmd_args))

        # Gradle and mozprocess do not get along well, so we use subprocess
        # directly.
        proc = subprocess.Popen(cmd_args, cwd=topsrcdir, stdout=devnull, stderr=devnull)
        status = None
        # Leave it to the subprocess to handle Ctrl+C. If it terminates as a result
        # of Ctrl+C, proc.wait() will return a status code, and, we get out of the
        # loop. If it doesn't, like e.g. gdb, we continue waiting.
        while status is None:
            try:
                status = proc.wait()
            except KeyboardInterrupt:
                pass

        try:
            proc.wait()
        except KeyboardInterrupt:
            proc.kill()
            raise


def api_lint(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_API_LINT_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    folder = lintargs['substs']['GRADLE_ANDROID_GECKOVIEW_APILINT_FOLDER']

    results = []

    with open(os.path.join(topobjdir, folder, 'apilint-result.json')) as f:
        issues = json.load(f)

        for rule in ('compat_failures', 'failures'):
            for r in issues[rule]:
                err = {
                    'rule': r['rule'] if rule == 'failures' else 'compat_failures',
                    'path': mozpath.relpath(r['file'], topsrcdir),
                    'lineno': int(r['line']),
                    'column': int(r.get('column') or 0),
                    'message': r['msg'],
                }
                results.append(result.from_config(config, **err))

        for r in issues['api_changes']:
            err = {
                'rule': 'api_changes',
                'path': mozpath.relpath(r['file'], topsrcdir),
                'lineno': int(r['line']),
                'column': int(r.get('column') or 0),
                'message': 'Unexpected api change. Please run ./gradlew {} for more '
                           'information'.format(
                    ' '.join(lintargs['substs']['GRADLE_ANDROID_API_LINT_TASKS'])),
            }
            results.append(result.from_config(config, **err))

    return results


def javadoc(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_GECKOVIEW_DOCS_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    output_files = lintargs['substs']['GRADLE_ANDROID_GECKOVIEW_DOCS_OUTPUT_FILES']

    results = []

    for output_file in output_files:
        with open(os.path.join(topobjdir, output_file)) as f:
            # Like: '[{"path":"/absolute/path/to/topsrcdir/mobile/android/geckoview/src/main/java/org/mozilla/geckoview/ContentBlocking.java","lineno":"462","level":"warning","message":"no @return"}]'.  # NOQA: E501
            issues = json.load(f)

            for issue in issues:
                issue['path'] = issue['path'].replace(lintargs['root'], '')
                # We want warnings to be errors for linting purposes.
                issue['level'] = 'error'
                results.append(result.from_config(config, **issue))

    return results


def lint(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_LINT_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    # It's surprising that this is the App variant name, but this is "withoutGeckoBinariesDebug"
    # right now and the GeckoView variant name is "withGeckoBinariesDebug".  This will be addressed
    # as we unify variants.
    path = os.path.join(
        lintargs['topobjdir'],
        'gradle/build/mobile/android/geckoview/reports',
        'lint-results-{}.xml'.format(lintargs['substs']['GRADLE_ANDROID_APP_VARIANT_NAME']))
    tree = ET.parse(open(path, 'rt'))
    root = tree.getroot()

    results = []

    for issue in root.findall('issue'):
        location = issue[0]
        err = {
            'level': issue.get('severity').lower(),
            'rule': issue.get('id'),
            'message': issue.get('message'),
            'path': location.get('file').replace(lintargs['root'], ''),
            'lineno': int(location.get('line') or 0),
        }
        results.append(result.from_config(config, **err))

    return results


def findbugs(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    # A brute force way to turn a Java FQN into a path on disk.  Assumes Java and Kotlin sources
    # are in mobile/android for performance and simplicity.
    sourcepath_finder = FileFinder(os.path.join(topsrcdir, 'mobile', 'android'))

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_FINDBUGS_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    path = os.path.join(
        lintargs['topobjdir'],
        'gradle/build/mobile/android/app/reports/findbugs',
        'findbugs-{}-output.xml'.format(lintargs['substs']['GRADLE_ANDROID_APP_VARIANT_NAME']))
    tree = ET.parse(open(path, 'rt'))
    root = tree.getroot()

    results = []

    for issue in root.findall('./BugInstance'):
        location = issue.find('./SourceLine')
        # Like 'org/mozilla/gecko/sync/repositories/android/FennecTabsRepository.java'.
        unanchored_sourcepath = location.get('sourcepath')

        sourcepaths = list(sourcepath_finder.find('**/{}'.format(unanchored_sourcepath)))
        if not len(sourcepaths) == 1:
            raise RuntimeError('No sourcepath found for unanchored sourcepath {path}'
                               .format(path=unanchored_sourcepath))

        sourcepath, _ = sourcepaths[0]

        err = {
            'level': 'error',
            'rule': issue.get('type'),
            'message': ET.tostring(issue),
            'path': os.path.join('mobile', 'android', sourcepath),
            'lineno': int(location.get('start') or 0),
        }
        results.append(result.from_config(config, **err))

    return results


def _parse_checkstyle_output(config, topsrcdir=None, report_path=None):
    tree = ET.parse(open(report_path, 'rt'))
    root = tree.getroot()

    for file in root.findall('file'):
        sourcepath = file.get('name').replace(topsrcdir + '/', '')

        for error in file.findall('error'):
            # Like <error column="42" line="22" message="Name 'mPorts' must match pattern 'xm[A-Z][A-Za-z]*$'." severity="error" source="com.puppycrawl.tools.checkstyle.checks.naming.MemberNameCheck" />.  # NOQA: E501
            err = {
                'level': 'error',
                'rule': error.get('source'),
                'message': error.get('message'),
                'path': sourcepath,
                'lineno': int(error.get('line') or 0),
                'column': int(error.get('column') or 0),
            }
            yield result.from_config(config, **err)


def checkstyle(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_CHECKSTYLE_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    results = []

    for relative_path in lintargs['substs']['GRADLE_ANDROID_CHECKSTYLE_OUTPUT_FILES']:
        report_path = os.path.join(lintargs['topobjdir'], relative_path)
        results.extend(_parse_checkstyle_output(config,
                                                topsrcdir=lintargs['root'],
                                                report_path=report_path))

    return results


def _parse_android_test_results(config, topsrcdir=None, report_dir=None):
    # A brute force way to turn a Java FQN into a path on disk.  Assumes Java
    # and Kotlin sources are in mobile/android for performance and simplicity.
    sourcepath_finder = FileFinder(os.path.join(topsrcdir, 'mobile', 'android'))

    finder = FileFinder(report_dir)
    reports = list(finder.find('TEST-*.xml'))
    if not reports:
        raise RuntimeError('No reports found under {}'.format(report_dir))

    for report, _ in reports:
        tree = ET.parse(open(os.path.join(finder.base, report), 'rt'))
        root = tree.getroot()

        class_name = root.get('name')  # Like 'org.mozilla.gecko.permissions.TestPermissions'.
        path = '**/' + class_name.replace('.', '/') + '.*'  # Like '**/org/mozilla/gecko/permissions/TestPermissions.*'.  # NOQA: E501

        for testcase in root.findall('testcase'):
            function_name = testcase.get('name')

            # Schema cribbed from http://llg.cubic.org/docs/junit/.
            for unexpected in itertools.chain(testcase.findall('error'),
                                              testcase.findall('failure')):
                sourcepaths = list(sourcepath_finder.find(path))
                if not sourcepaths:
                    raise RuntimeError('No sourcepath found for class {class_name}'
                                       .format(class_name=class_name))

                for sourcepath, _ in sourcepaths:
                    lineno = 0
                    message = unexpected.get('message')
                    # Turn '... at org.mozilla.gecko.permissions.TestPermissions.testMultipleRequestsAreQueuedAndDispatchedSequentially(TestPermissions.java:118)' into 118.  # NOQA: E501
                    pattern = r'at {class_name}\.{function_name}\(.*:(\d+)\)'
                    pattern = pattern.format(class_name=class_name, function_name=function_name)
                    match = re.search(pattern, message)
                    if match:
                        lineno = int(match.group(1))
                    else:
                        msg = 'No source line found for {class_name}.{function_name}'.format(
                            class_name=class_name, function_name=function_name)
                        raise RuntimeError(msg)

                    err = {
                        'level': 'error',
                        'rule': unexpected.get('type'),
                        'message': message,
                        'path': os.path.join('mobile', 'android', sourcepath),
                        'lineno': lineno,
                    }
                    yield result.from_config(config, **err)


def test(config, **lintargs):
    topsrcdir = lintargs['root']
    topobjdir = lintargs['topobjdir']

    gradle(topsrcdir=topsrcdir, topobjdir=topobjdir,
           tasks=lintargs['substs']['GRADLE_ANDROID_TEST_TASKS'],
           extra_args=lintargs.get('extra_args') or [])

    results = []

    def capitalize(s):
        # Can't use str.capitalize because it lower cases trailing letters.
        return (s[0].upper() + s[1:]) if s else ''

    pairs = (('app', lintargs['substs']['GRADLE_ANDROID_APP_VARIANT_NAME']),
             ('geckoview', lintargs['substs']['GRADLE_ANDROID_GECKOVIEW_VARIANT_NAME']))
    for project, variant in pairs:
        report_dir = os.path.join(
            lintargs['topobjdir'],
            'gradle/build/mobile/android/{}/test-results/test{}UnitTest'
            .format(project, capitalize(variant)))
    results.extend(_parse_android_test_results(config,
                                               topsrcdir=lintargs['root'],
                                               report_dir=report_dir))

    return results
