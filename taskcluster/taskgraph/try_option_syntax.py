# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import copy
import logging
import re
import shlex

logger = logging.getLogger(__name__)

TRY_DELIMITER = 'try:'

# The build type aliases are very cryptic and only used in try flags these are
# mappings from the single char alias to a longer more recognizable form.
BUILD_TYPE_ALIASES = {
    'o': 'opt',
    'd': 'debug'
}

# consider anything in this whitelist of kinds to be governed by -b/-p
BUILD_KINDS = set([
    'build',
    'artifact-build',
    'hazard',
    'l10n',
    'valgrind',
    'static-analysis',
    'spidermonkey',
])


# mapping from shortcut name (usable with -u) to a boolean function identifying
# matching test names
def alias_prefix(prefix):
    return lambda name: name.startswith(prefix)


def alias_contains(infix):
    return lambda name: infix in name


def alias_matches(pattern):
    pattern = re.compile(pattern)
    return lambda name: pattern.match(name)


UNITTEST_ALIASES = {
    # Aliases specify shorthands that can be used in try syntax.  The shorthand
    # is the dictionary key, with the value representing a pattern for matching
    # unittest_try_names.
    #
    # Note that alias expansion is performed in the absence of any chunk
    # prefixes.  For example, the first example above would replace "foo-7"
    # with "foobar-7".  Note that a few aliases allowed chunks to be specified
    # without a leading `-`, for example 'mochitest-dt1'. That's no longer
    # supported.
    'cppunit': alias_prefix('cppunit'),
    'crashtest': alias_prefix('crashtest'),
    'crashtest-e10s': alias_prefix('crashtest-e10s'),
    'e10s': alias_contains('e10s'),
    'external-media-tests': alias_prefix('external-media-tests'),
    'firefox-ui-functional': alias_prefix('firefox-ui-functional'),
    'firefox-ui-functional-e10s': alias_prefix('firefox-ui-functional-e10s'),
    'gaia-js-integration': alias_contains('gaia-js-integration'),
    'gtest': alias_prefix('gtest'),
    'jittest': alias_prefix('jittest'),
    'jittests': alias_prefix('jittest'),
    'jsreftest': alias_prefix('jsreftest'),
    'jsreftest-e10s': alias_prefix('jsreftest-e10s'),
    'marionette': alias_prefix('marionette'),
    'marionette-e10s': alias_prefix('marionette-e10s'),
    'mochitest': alias_prefix('mochitest'),
    'mochitests': alias_prefix('mochitest'),
    'mochitest-e10s': alias_prefix('mochitest-e10s'),
    'mochitests-e10s': alias_prefix('mochitest-e10s'),
    'mochitest-debug': alias_prefix('mochitest-debug-'),
    'mochitest-a11y': alias_contains('mochitest-a11y'),
    'mochitest-bc': alias_prefix('mochitest-browser-chrome'),
    'mochitest-e10s-bc': alias_prefix('mochitest-browser-chrome-e10s'),
    'mochitest-browser-chrome': alias_prefix('mochitest-browser-chrome'),
    'mochitest-e10s-browser-chrome': alias_prefix('mochitest-browser-chrome-e10s'),
    'mochitest-chrome': alias_contains('mochitest-chrome'),
    'mochitest-dt': alias_prefix('mochitest-devtools-chrome'),
    'mochitest-e10s-dt': alias_prefix('mochitest-devtools-chrome-e10s'),
    'mochitest-gl': alias_prefix('mochitest-webgl'),
    'mochitest-gl-e10s': alias_prefix('mochitest-webgl-e10s'),
    'mochitest-gpu': alias_prefix('mochitest-gpu'),
    'mochitest-gpu-e10s': alias_prefix('mochitest-gpu-e10s'),
    'mochitest-clipboard': alias_prefix('mochitest-clipboard'),
    'mochitest-clipboard-e10s': alias_prefix('mochitest-clipboard-e10s'),
    'mochitest-jetpack': alias_prefix('mochitest-jetpack'),
    'mochitest-media': alias_prefix('mochitest-media'),
    'mochitest-media-e10s': alias_prefix('mochitest-media-e10s'),
    'mochitest-vg': alias_prefix('mochitest-valgrind'),
    'reftest': alias_matches(r'^(plain-)?reftest.*$'),
    'reftest-no-accel': alias_matches(r'^(plain-)?reftest-no-accel.*$'),
    'reftests': alias_matches(r'^(plain-)?reftest.*$'),
    'reftests-e10s': alias_matches(r'^(plain-)?reftest-e10s.*$'),
    'reftest-stylo': alias_matches(r'^(plain-)?reftest-stylo.*$'),
    'robocop': alias_prefix('robocop'),
    'web-platform-test': alias_prefix('web-platform-tests'),
    'web-platform-tests': alias_prefix('web-platform-tests'),
    'web-platform-tests-e10s': alias_prefix('web-platform-tests-e10s'),
    'web-platform-tests-reftests': alias_prefix('web-platform-tests-reftests'),
    'web-platform-tests-reftests-e10s': alias_prefix('web-platform-tests-reftests-e10s'),
    'xpcshell': alias_prefix('xpcshell'),
}

# unittest platforms can be specified by substring of the "pretty name", which
# is basically the old Buildbot builder name.  This dict has {pretty name,
# [test_platforms]} translations, This includes only the most commonly-used
# substrings.  This is intended only for backward-compatibility.  New test
# platforms should have their `test_platform` spelled out fully in try syntax.
# Note that the test platforms here are only the prefix up to the `/`.
UNITTEST_PLATFORM_PRETTY_NAMES = {
    'Ubuntu': ['linux32', 'linux64', 'linux64-asan'],
    'x64': ['linux64', 'linux64-asan'],
    'Android 4.3': ['android-4.3-arm7-api-15'],
    '10.10': ['macosx64'],
    # other commonly-used substrings for platforms not yet supported with
    # in-tree taskgraphs:
    # '10.10.5': [..TODO..],
    # '10.6': [..TODO..],
    # '10.8': [..TODO..],
    # 'Android 2.3 API9': [..TODO..],
    # 'Windows 7':  [..TODO..],
    # 'Windows 7 VM': [..TODO..],
    # 'Windows 8':  [..TODO..],
    # 'Windows XP': [..TODO..],
    # 'win32': [..TODO..],
    # 'win64': [..TODO..],
}

# We have a few platforms for which we want to do some "extra" builds, or at
# least build-ish things.  Sort of.  Anyway, these other things are implemented
# as different "platforms".  These do *not* automatically ride along with "-p
# all"
RIDEALONG_BUILDS = {
    'android-api-15': [
        'android-api-15-l10n',
    ],
    'linux': [
        'linux-l10n',
    ],
    'linux64': [
        'linux64-l10n',
        'sm-plain',
        'sm-nonunified',
        'sm-arm-sim',
        'sm-arm64-sim',
        'sm-compacting',
        'sm-rootanalysis',
        'sm-package',
        'sm-tsan',
        'sm-asan',
        'sm-mozjs-sys',
        'sm-msan',
        'sm-fuzzing',
    ],
}

TEST_CHUNK_SUFFIX = re.compile('(.*)-([0-9]+)$')


def escape_whitespace_in_brackets(input_str):
    '''
    In tests you may restrict them by platform [] inside of the brackets
    whitespace may occur this is typically invalid shell syntax so we escape it
    with backslash sequences    .
    '''
    result = ""
    in_brackets = False
    for char in input_str:
        if char == '[':
            in_brackets = True
            result += char
            continue

        if char == ']':
            in_brackets = False
            result += char
            continue

        if char == ' ' and in_brackets:
            result += '\ '
            continue

        result += char

    return result


def split_try_msg(message):
    try:
        try_idx = message.index('try:')
    except ValueError:
        return []
    message = message[try_idx:].split('\n')[0]
    # shlex used to ensure we split correctly when giving values to argparse.
    return shlex.split(escape_whitespace_in_brackets(message))


def parse_message(message):
    parts = split_try_msg(message)

    # Argument parser based on try flag flags
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--build', dest='build_types')
    parser.add_argument('-p', '--platform', nargs='?',
                        dest='platforms', const='all', default='all')
    parser.add_argument('-u', '--unittests', nargs='?',
                        dest='unittests', const='all', default='all')
    parser.add_argument('-t', '--talos', nargs='?', dest='talos', const='all', default='none')
    parser.add_argument('-i', '--interactive',
                        dest='interactive', action='store_true', default=False)
    parser.add_argument('-e', '--all-emails',
                        dest='notifications', action='store_const', const='all')
    parser.add_argument('-f', '--failure-emails',
                        dest='notifications', action='store_const', const='failure')
    parser.add_argument('-j', '--job', dest='jobs', action='append')
    parser.add_argument('--rebuild-talos', dest='talos_trigger_tests', action='store',
                        type=int, default=1)
    parser.add_argument('--setenv', dest='env', action='append')
    parser.add_argument('--geckoProfile', dest='profile', action='store_true')
    parser.add_argument('--tag', dest='tag', action='store', default=None)
    parser.add_argument('--no-retry', dest='no_retry', action='store_true')
    parser.add_argument('--include-nightly', dest='include_nightly', action='store_true')

    # While we are transitioning from BB to TC, we want to push jobs to tc-worker
    # machines but not overload machines with every try push. Therefore, we add
    # this temporary option to be able to push jobs to tc-worker.
    parser.add_argument('-w', '--taskcluster-worker',
                        dest='taskcluster_worker', action='store_true', default=False)

    # In order to run test jobs multiple times
    parser.add_argument('--rebuild', dest='trigger_tests', type=int, default=1)
    args, _ = parser.parse_known_args(parts)
    return args


class TryOptionSyntax(object):

    def __init__(self, message, full_task_graph):
        """
        Parse a "try syntax" formatted commit message.  This is the old "-b do -p
        win32 -u all" format.  Aliases are applied to map short names to full
        names.

        The resulting object has attributes:

        - build_types: a list containing zero or more of 'opt' and 'debug'
        - platforms: a list of selected platform names, or None for all
        - unittests: a list of tests, of the form given below, or None for all
        - jobs: a list of requested job names, or None for all
        - trigger_tests: the number of times tests should be triggered (--rebuild)
        - interactive: true if --interactive
        - notifications: either None if no notifications or one of 'all' or 'failure'
        - talos_trigger_tests: the number of time talos tests should be triggered (--rebuild-talos)
        - env: additional environment variables (ENV=value)
        - profile: run talos in profile mode
        - tag: restrict tests to the specified tag
        - no_retry: do not retry failed jobs

        The unittests and talos lists contain dictionaries of the form:

        {
            'test': '<suite name>',
            'platforms': [..platform names..], # to limit to only certain platforms
            'only_chunks': set([..chunk numbers..]), # to limit only to certain chunks
        }
        """
        self.jobs = []
        self.build_types = []
        self.platforms = []
        self.unittests = []
        self.talos = []
        self.trigger_tests = 0
        self.interactive = False
        self.notifications = None
        self.talos_trigger_tests = 0
        self.env = []
        self.profile = False
        self.tag = None
        self.no_retry = False

        parts = split_try_msg(message)
        if not parts:
            return None

        args = parse_message(message)
        assert args is not None

        self.jobs = self.parse_jobs(args.jobs)
        self.build_types = self.parse_build_types(args.build_types, full_task_graph)
        self.platforms = self.parse_platforms(args.platforms, full_task_graph)
        self.unittests = self.parse_test_option(
            "unittest_try_name", args.unittests, full_task_graph)
        self.talos = self.parse_test_option("talos_try_name", args.talos, full_task_graph)
        self.trigger_tests = args.trigger_tests
        self.interactive = args.interactive
        self.notifications = args.notifications
        self.talos_trigger_tests = args.talos_trigger_tests
        self.env = args.env
        self.profile = args.profile
        self.tag = args.tag
        self.no_retry = args.no_retry
        self.include_nightly = args.include_nightly

    def parse_jobs(self, jobs_arg):
        if not jobs_arg or jobs_arg == ['all']:
            return None
        expanded = []
        for job in jobs_arg:
            expanded.extend(j.strip() for j in job.split(','))
        return expanded

    def parse_build_types(self, build_types_arg, full_task_graph):
        if build_types_arg is None:
            build_types_arg = []

        build_types = filter(None, [BUILD_TYPE_ALIASES.get(build_type) for
                             build_type in build_types_arg])

        all_types = set(t.attributes['build_type']
                        for t in full_task_graph.tasks.itervalues()
                        if 'build_type' in t.attributes)
        bad_types = set(build_types) - all_types
        if bad_types:
            raise Exception("Unknown build type(s) [%s] specified for try" % ','.join(bad_types))

        return build_types

    def parse_platforms(self, platform_arg, full_task_graph):
        if platform_arg == 'all':
            return None

        results = []
        for build in platform_arg.split(','):
            results.append(build)
            if build in RIDEALONG_BUILDS:
                results.extend(RIDEALONG_BUILDS[build])
                logger.info("platform %s triggers ridealong builds %s" %
                            (build, ', '.join(RIDEALONG_BUILDS[build])))

        test_platforms = set(t.attributes['test_platform']
                             for t in full_task_graph.tasks.itervalues()
                             if 'test_platform' in t.attributes)
        build_platforms = set(t.attributes['build_platform']
                              for t in full_task_graph.tasks.itervalues()
                              if 'build_platform' in t.attributes)
        all_platforms = test_platforms | build_platforms
        bad_platforms = set(results) - all_platforms
        if bad_platforms:
            raise Exception("Unknown platform(s) [%s] specified for try" % ','.join(bad_platforms))

        return results

    def parse_test_option(self, attr_name, test_arg, full_task_graph):
        '''

        Parse a unittest (-u) or talos (-t) option, in the context of a full
        task graph containing available `unittest_try_name` or `talos_try_name`
        attributes.  There are three cases:

            - test_arg is == 'none' (meaning an empty list)
            - test_arg is == 'all' (meaning use the list of jobs for that job type)
            - test_arg is comma string which needs to be parsed
        '''

        # Empty job list case...
        if test_arg is None or test_arg == 'none':
            return []

        all_platforms = set(t.attributes['test_platform'].split('/')[0]
                            for t in full_task_graph.tasks.itervalues()
                            if 'test_platform' in t.attributes)

        tests = self.parse_test_opts(test_arg, all_platforms)

        if not tests:
            return []

        all_tests = set(t.attributes[attr_name]
                        for t in full_task_graph.tasks.itervalues()
                        if attr_name in t.attributes)

        # Special case where tests is 'all' and must be expanded
        if tests[0]['test'] == 'all':
            results = []
            all_entry = tests[0]
            for test in all_tests:
                entry = {'test': test}
                # If there are platform restrictions copy them across the list.
                if 'platforms' in all_entry:
                    entry['platforms'] = list(all_entry['platforms'])
                results.append(entry)
            return self.parse_test_chunks(all_tests, results)
        else:
            return self.parse_test_chunks(all_tests, tests)

    def parse_test_opts(self, input_str, all_platforms):
        '''
        Parse `testspec,testspec,..`, where each testspec is a test name
        optionally followed by a list of test platforms or negated platforms in
        `[]`.

        No brackets indicates that tests should run on all platforms for which
        builds are available.  If testspecs are provided, then each is treated,
        from left to right, as an instruction to include or (if negated)
        exclude a set of test platforms.  A single spec may expand to multiple
        test platforms via UNITTEST_PLATFORM_PRETTY_NAMES.  If the first test
        spec is negated, processing begins with the full set of available test
        platforms; otherwise, processing begins with an empty set of test
        platforms.
        '''

        # Final results which we will return.
        tests = []

        cur_test = {}
        token = ''
        in_platforms = False

        def normalize_platforms():
            if 'platforms' not in cur_test:
                return
            # if the first spec is a negation, start with all platforms
            if cur_test['platforms'][0][0] == '-':
                platforms = all_platforms.copy()
            else:
                platforms = []
            for platform in cur_test['platforms']:
                if platform[0] == '-':
                    platforms = [p for p in platforms if p != platform[1:]]
                else:
                    platforms.append(platform)
            cur_test['platforms'] = platforms

        def add_test(value):
            normalize_platforms()
            cur_test['test'] = value.strip()
            tests.insert(0, cur_test)

        def add_platform(value):
            platform = value.strip()
            if platform[0] == '-':
                negated = True
                platform = platform[1:]
            else:
                negated = False
            platforms = UNITTEST_PLATFORM_PRETTY_NAMES.get(platform, [platform])
            if negated:
                platforms = ["-" + p for p in platforms]
            cur_test['platforms'] = platforms + cur_test.get('platforms', [])

        # This might be somewhat confusing but we parse the string _backwards_ so
        # there is no ambiguity over what state we are in.
        for char in reversed(input_str):

            # , indicates exiting a state
            if char == ',':

                # Exit a particular platform.
                if in_platforms:
                    add_platform(token)

                # Exit a particular test.
                else:
                    add_test(token)
                    cur_test = {}

                # Token must always be reset after we exit a state
                token = ''
            elif char == '[':
                # Exiting platform state entering test state.
                add_platform(token)
                token = ''
                in_platforms = False
            elif char == ']':
                # Entering platform state.
                in_platforms = True
            else:
                # Accumulator.
                token = char + token

        # Handle any left over tokens.
        if token:
            add_test(token)

        return tests

    def handle_alias(self, test, all_tests):
        '''
        Expand a test if its name refers to an alias, returning a list of test
        dictionaries cloned from the first (to maintain any metadata).
        '''
        if test['test'] not in UNITTEST_ALIASES:
            return [test]

        alias = UNITTEST_ALIASES[test['test']]

        def mktest(name):
            newtest = copy.deepcopy(test)
            newtest['test'] = name
            return newtest

        def exprmatch(alias):
            return [t for t in all_tests if alias(t)]

        return [mktest(t) for t in exprmatch(alias)]

    def parse_test_chunks(self, all_tests, tests):
        '''
        Test flags may include parameters to narrow down the number of chunks in a
        given push. We don't model 1 chunk = 1 job in taskcluster so we must check
        each test flag to see if it is actually specifying a chunk.
        '''
        results = []
        seen_chunks = {}
        for test in tests:
            matches = TEST_CHUNK_SUFFIX.match(test['test'])
            if matches:
                name = matches.group(1)
                chunk = matches.group(2)
                if name in seen_chunks:
                    seen_chunks[name].add(chunk)
                else:
                    seen_chunks[name] = {chunk}
                    test['test'] = name
                    test['only_chunks'] = seen_chunks[name]
                    results.append(test)
            else:
                results.extend(self.handle_alias(test, all_tests))

        # uniquify the results over the test names
        results = {test['test']: test for test in results}.values()
        return results

    def find_all_attribute_suffixes(self, graph, prefix):
        rv = set()
        for t in graph.tasks.itervalues():
            for a in t.attributes:
                if a.startswith(prefix):
                    rv.add(a[len(prefix):])
        return sorted(rv)

    def task_matches(self, task):
        attr = task.attributes.get

        def check_run_on_projects():
            if attr('nightly') and not self.include_nightly:
                return False
            return set(['try', 'all']) & set(attr('run_on_projects', []))

        def match_test(try_spec, attr_name):
            if attr('build_type') not in self.build_types:
                return False
            if self.platforms is not None:
                if attr('build_platform') not in self.platforms:
                    return False
            else:
                if not check_run_on_projects():
                    return False
            if try_spec is None:
                return True
            # TODO: optimize this search a bit
            for test in try_spec:
                if attr(attr_name) == test['test']:
                    break
            else:
                return False
            if 'platforms' in test:
                platform = attr('test_platform', '').split('/')[0]
                if platform not in test['platforms']:
                    return False
            if 'only_chunks' in test and attr('test_chunk') not in test['only_chunks']:
                return False
            return True

        job_try_name = attr('job_try_name')
        if job_try_name:
            # Beware the subtle distinction between [] and None for self.jobs and self.platforms.
            # They will be [] if there was no try syntax, and None if try syntax was detected but
            # they remained unspecified.
            if self.jobs and job_try_name not in self.jobs:
                return False
            elif not self.jobs and 'build' in task.dependencies:
                # We exclude tasks with build dependencies from the default set of jobs because
                # they will schedule their builds even if they end up optimized away. This means
                # to run these tasks on try, they'll need to be explicitly specified by -j until
                # we find a better solution (see bug 1372510).
                return False
            elif not self.jobs and attr('build_platform'):
                if self.platforms is None or attr('build_platform') in self.platforms:
                    return True
                return False
            return True
        elif attr('kind') == 'test':
            return match_test(self.unittests, 'unittest_try_name') \
                 or match_test(self.talos, 'talos_try_name')
        elif attr('kind') in BUILD_KINDS:
            if attr('build_type') not in self.build_types:
                return False
            elif self.platforms is None:
                # for "-p all", look for try in the 'run_on_projects' attribute
                return check_run_on_projects()
            else:
                if attr('build_platform') not in self.platforms:
                    return False
            return True
        else:
            return False

    def __str__(self):
        def none_for_all(list):
            if list is None:
                return '<all>'
            return ', '.join(str(e) for e in list)

        return "\n".join([
            "build_types: " + ", ".join(self.build_types),
            "platforms: " + none_for_all(self.platforms),
            "unittests: " + none_for_all(self.unittests),
            "talos: " + none_for_all(self.talos),
            "jobs: " + none_for_all(self.jobs),
            "trigger_tests: " + str(self.trigger_tests),
            "interactive: " + str(self.interactive),
            "notifications: " + str(self.notifications),
            "talos_trigger_tests: " + str(self.talos_trigger_tests),
            "env: " + str(self.env),
            "profile: " + str(self.profile),
            "tag: " + str(self.tag),
            "no_retry: " + str(self.no_retry),
        ])
