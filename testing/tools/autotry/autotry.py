# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import itertools
import os
import re
import subprocess
import sys
import which

from collections import defaultdict

import ConfigParser


def arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('paths', nargs='*', help='Paths to search for tests to run on try.')
    parser.add_argument('-b', '--build', dest='builds', default='do',
                        help='Build types to run (d for debug, o for optimized).')
    parser.add_argument('-p', '--platform', dest='platforms', action='append',
                        help='Platforms to run (required if not found in the environment as AUTOTRY_PLATFORM_HINT).')
    parser.add_argument('-u', '--unittests', dest='tests', action='append',
                        help='Test suites to run in their entirety.')
    parser.add_argument('-t', '--talos', dest='talos', action='append',
                        help='Talos suites to run.')
    parser.add_argument('-j', '--jobs', dest='jobs', action='append',
                        help='Job tasks to run.')
    parser.add_argument('--tag', dest='tags', action='append',
                        help='Restrict tests to the given tag (may be specified multiple times).')
    parser.add_argument('--and', action='store_true', dest='intersection',
                        help='When -u and paths are supplied run only the intersection of the tests specified by the two arguments.')
    parser.add_argument('--no-push', dest='push', action='store_false',
                        help='Do not push to try as a result of running this command (if '
                        'specified this command will only print calculated try '
                        'syntax and selection info).')
    parser.add_argument('--save', dest='save', action='store',
                        help='Save the command line arguments for future use with --preset.')
    parser.add_argument('--preset', dest='load', action='store',
                        help='Load a saved set of arguments. Additional arguments will override saved ones.')
    parser.add_argument('--list', action='store_true',
                        help='List all saved try strings')
    parser.add_argument('-v', '--verbose', dest='verbose', action='store_true', default=False,
                        help='Print detailed information about the resulting test selection '
                        'and commands performed.')
    for arg, opts in AutoTry.pass_through_arguments.items():
        parser.add_argument(arg, **opts)
    return parser

class TryArgumentTokenizer(object):
    symbols = [("seperator", ","),
               ("list_start", "\["),
               ("list_end", "\]"),
               ("item", "([^,\[\]\s][^,\[\]]+)"),
               ("space", "\s+")]
    token_re = re.compile("|".join("(?P<%s>%s)" % item for item in symbols))

    def tokenize(self, data):
        for match in self.token_re.finditer(data):
            symbol = match.lastgroup
            data = match.group(symbol)
            if symbol == "space":
                pass
            else:
                yield symbol, data

class TryArgumentParser(object):
    """Simple three-state parser for handling expressions
    of the from "foo[sub item, another], bar,baz". This takes
    input from the TryArgumentTokenizer and runs through a small
    state machine, returning a dictionary of {top-level-item:[sub_items]}
    i.e. the above would result in
    {"foo":["sub item", "another"], "bar": [], "baz": []}
    In the case of invalid input a ValueError is raised."""

    EOF = object()

    def __init__(self):
        self.reset()

    def reset(self):
        self.tokens = None
        self.current_item = None
        self.data = {}
        self.token = None
        self.state = None

    def parse(self, tokens):
        self.reset()
        self.tokens = tokens
        self.consume()
        self.state = self.item_state
        while self.token[0] != self.EOF:
            self.state()
        return self.data

    def consume(self):
        try:
            self.token = self.tokens.next()
        except StopIteration:
            self.token = (self.EOF, None)

    def expect(self, *types):
        if self.token[0] not in types:
            raise ValueError("Error parsing try string, unexpected %s" % (self.token[0]))

    def item_state(self):
        self.expect("item")
        value = self.token[1].strip()
        if value not in self.data:
            self.data[value] = []
        self.current_item = value
        self.consume()
        if self.token[0] == "seperator":
            self.consume()
        elif self.token[0] == "list_start":
            self.consume()
            self.state = self.subitem_state
        elif self.token[0] == self.EOF:
            pass
        else:
            raise ValueError

    def subitem_state(self):
        self.expect("item")
        value = self.token[1].strip()
        self.data[self.current_item].append(value)
        self.consume()
        if self.token[0] == "seperator":
            self.consume()
        elif self.token[0] == "list_end":
            self.consume()
            self.state = self.after_list_end_state
        else:
            raise ValueError

    def after_list_end_state(self):
        self.expect("seperator")
        self.consume()
        self.state = self.item_state

def parse_arg(arg):
    tokenizer = TryArgumentTokenizer()
    parser = TryArgumentParser()
    return parser.parse(tokenizer.tokenize(arg))

class AutoTry(object):

    # Maps from flavors to the job names needed to run that flavour
    flavor_jobs = {
        'mochitest': ['mochitest-1', 'mochitest-e10s-1'],
        'xpcshell': ['xpcshell'],
        'chrome': ['mochitest-o'],
        'browser-chrome': ['mochitest-browser-chrome-1',
                           'mochitest-e10s-browser-chrome-1',
                           'mochitest-browser-chrome-e10s-1'],
        'devtools-chrome': ['mochitest-devtools-chrome-1',
                            'mochitest-e10s-devtools-chrome-1',
                            'mochitest-devtools-chrome-e10s-1'],
        'crashtest': ['crashtest', 'crashtest-e10s'],
        'reftest': ['reftest', 'reftest-e10s'],
        'web-platform-tests': ['web-platform-tests-1'],
    }

    flavor_suites = {
        "mochitest": "mochitests",
        "xpcshell": "xpcshell",
        "chrome": "mochitest-o",
        "browser-chrome": "mochitest-bc",
        "devtools-chrome": "mochitest-dt",
        "crashtest": "crashtest",
        "reftest": "reftest",
        "web-platform-tests": "web-platform-tests",
    }

    compiled_suites = [
        "cppunit",
        "gtest",
        "jittest",
    ]

    common_suites = [
        "cppunit",
        "crashtest",
        "firefox-ui-functional",
        "gtest",
        "jittest",
        "jsreftest",
        "marionette",
        "marionette-e10s",
        "media-tests",
        "mochitests",
        "reftest",
        "web-platform-tests",
        "xpcshell",
    ]

    # Arguments we will accept on the command line and pass through to try
    # syntax with no further intervention. The set is taken from
    # http://trychooser.pub.build.mozilla.org with a few additions.
    #
    # Note that the meaning of store_false and store_true arguments is
    # not preserved here, as we're only using these to echo the literal
    # arguments to another consumer. Specifying either store_false or
    # store_true here will have an equivalent effect.
    pass_through_arguments = {
        '--rebuild': {
            'action': 'store',
            'dest': 'rebuild',
            'help': 'Re-trigger all test jobs (up to 20 times)',
        },
        '--rebuild-talos': {
            'action': 'store',
            'dest': 'rebuild_talos',
            'help': 'Re-trigger all talos jobs',
        },
        '--interactive': {
            'action': 'store_true',
            'dest': 'interactive',
            'help': 'Allow ssh-like access to running test containers',
        },
        '--no-retry': {
            'action': 'store_true',
            'dest': 'no_retry',
            'help': 'Do not retrigger failed tests',
        },
        '--setenv': {
            'action': 'append',
            'dest': 'setenv',
            'help': 'Set the corresponding variable in the test environment for'
                    'applicable harnesses.',
        },
        '-f': {
            'action': 'store_true',
            'dest': 'failure_emails',
            'help': 'Request failure emails only',
        },
        '--failure-emails': {
            'action': 'store_true',
            'dest': 'failure_emails',
            'help': 'Request failure emails only',
        },
        '-e': {
            'action': 'store_true',
            'dest': 'all_emails',
            'help': 'Request all emails',
        },
        '--all-emails': {
            'action': 'store_true',
            'dest': 'all_emails',
            'help': 'Request all emails',
        },
        '--artifact': {
            'action': 'store_true',
            'dest': 'artifact',
            'help': 'Force artifact builds where possible.',
        },
        '--upload-xdbs': {
            'action': 'store_true',
            'dest': 'upload_xdbs',
            'help': 'Upload XDB compilation db files generated by hazard build',
        },
    }

    def __init__(self, topsrcdir, resolver_func, mach_context):
        self.topsrcdir = topsrcdir
        self._resolver_func = resolver_func
        self._resolver = None
        self.mach_context = mach_context

        if os.path.exists(os.path.join(self.topsrcdir, '.hg')):
            self._use_git = False
        else:
            self._use_git = True

    @property
    def resolver(self):
        if self._resolver is None:
            self._resolver = self._resolver_func()
        return self._resolver

    @property
    def config_path(self):
        return os.path.join(self.mach_context.state_dir, "autotry.ini")

    def load_config(self, name):
        config = ConfigParser.RawConfigParser()
        success = config.read([self.config_path])
        if not success:
            return None

        try:
            data = config.get("try", name)
        except (ConfigParser.NoSectionError, ConfigParser.NoOptionError):
            return None

        kwargs = vars(arg_parser().parse_args(self.split_try_string(data)))

        return kwargs

    def list_presets(self):
        config = ConfigParser.RawConfigParser()
        success = config.read([self.config_path])

        data = []
        if success:
            try:
                data = config.items("try")
            except (ConfigParser.NoSectionError, ConfigParser.NoOptionError):
                pass

        if not data:
            print("No presets found")

        for name, try_string in data:
            print("%s: %s" % (name, try_string))

    def split_try_string(self, data):
        return re.findall(r'(?:\[.*?\]|\S)+', data)

    def save_config(self, name, data):
        assert data.startswith("try: ")
        data = data[len("try: "):]

        parser = ConfigParser.RawConfigParser()
        parser.read([self.config_path])

        if not parser.has_section("try"):
            parser.add_section("try")

        parser.set("try", name, data)

        with open(self.config_path, "w") as f:
            parser.write(f)

    def paths_by_flavor(self, paths=None, tags=None):
        paths_by_flavor = defaultdict(set)

        if not (paths or tags):
            return dict(paths_by_flavor)

        tests = list(self.resolver.resolve_tests(paths=paths,
                                                 tags=tags))

        for t in tests:
            if t['flavor'] in self.flavor_suites:
                flavor = t['flavor']
                if 'subsuite' in t and t['subsuite'] == 'devtools':
                    flavor = 'devtools-chrome'

                if flavor in ['crashtest', 'reftest']:
                    manifest_relpath = os.path.relpath(t['manifest'], self.topsrcdir)
                    paths_by_flavor[flavor].add(os.path.dirname(manifest_relpath))
                elif 'dir_relpath' in t:
                    paths_by_flavor[flavor].add(t['dir_relpath'])
                else:
                    file_relpath = os.path.relpath(t['path'], self.topsrcdir)
                    dir_relpath = os.path.dirname(file_relpath)
                    paths_by_flavor[flavor].add(dir_relpath)

        for flavor, path_set in paths_by_flavor.items():
            paths_by_flavor[flavor] = self.deduplicate_prefixes(path_set, paths)

        return dict(paths_by_flavor)

    def deduplicate_prefixes(self, path_set, input_paths):
        # Removes paths redundant to test selection in the given path set.
        # If a path was passed on the commandline that is the prefix of a
        # path in our set, we only need to include the specified prefix to
        # run the intended tests (every test in "layout/base" will run if
        # "layout" is passed to the reftest harness).
        removals = set()
        additions = set()

        for path in path_set:
            full_path = path
            while path:
                path, _ = os.path.split(path)
                if path in input_paths:
                    removals.add(full_path)
                    additions.add(path)

        return additions | (path_set - removals)

    def remove_duplicates(self, paths_by_flavor, tests):
        rv = {}
        for item in paths_by_flavor:
            if self.flavor_suites[item] not in tests:
                rv[item] = paths_by_flavor[item].copy()
        return rv

    def calc_try_syntax(self, platforms, tests, talos, jobs, builds, paths_by_flavor, tags,
                        extras, intersection):
        parts = ["try:"]

        if platforms:
            parts.extend(["-b", builds, "-p", ",".join(platforms)])

        suites = tests if not intersection else {}
        paths = set()
        for flavor, flavor_tests in paths_by_flavor.iteritems():
            suite = self.flavor_suites[flavor]
            if suite not in suites and (not intersection or suite in tests):
                for job_name in self.flavor_jobs[flavor]:
                    for test in flavor_tests:
                        paths.add("%s:%s" % (flavor, test))
                    suites[job_name] = tests.get(suite, [])

        # intersection implies tests are expected
        if intersection and not suites:
            raise ValueError("No tests found matching filters")

        if extras.get('artifact'):
            rejected = []
            for suite in suites.keys():
                if any([suite.startswith(c) for c in self.compiled_suites]):
                    rejected.append(suite)
            if rejected:
                raise ValueError("You can't run {} with "
                                 "--artifact option.".format(', '.join(rejected)))

        if extras.get('artifact') and 'all' in suites.keys():
            non_compiled_suites = set(self.common_suites) - set(self.compiled_suites)
            message = ('You asked for |-u all| with |--artifact| but compiled-code tests ({tests})'
                       ' can\'t run against an artifact build. Running (-u {non_compiled_suites}) '
                       'instead.')
            string_format = {
                'tests': ','.join(self.compiled_suites),
                'non_compiled_suites': ','.join(non_compiled_suites),
            }
            print(message.format(**string_format))
            del suites['all']
            suites.update({suite_name: None for suite_name in non_compiled_suites})

        if suites:
            parts.append("-u")
            parts.append(",".join("%s%s" % (k, "[%s]" % ",".join(v) if v else "")
                                  for k,v in sorted(suites.items())))

        if talos:
            parts.append("-t")
            parts.append(",".join("%s%s" % (k, "[%s]" % ",".join(v) if v else "")
                                  for k,v in sorted(talos.items())))

        if jobs:
            parts.append("-j")
            parts.append(",".join(jobs))

        if tags:
            parts.append(' '.join('--tag %s' % t for t in tags))

        if paths:
            parts.append("--try-test-paths %s" % " ".join(sorted(paths)))

        args_by_dest = {v['dest']: k for k, v in AutoTry.pass_through_arguments.items()}
        for dest, value in extras.iteritems():
            assert dest in args_by_dest
            arg = args_by_dest[dest]
            action = AutoTry.pass_through_arguments[arg]['action']
            if action == 'store':
                parts.append(arg)
                parts.append(value)
            if action == 'append':
                for e in value:
                    parts.append(arg)
                    parts.append(e)
            if action in ('store_true', 'store_false'):
                parts.append(arg)

        try_syntax = " ".join(parts)
        return try_syntax

    def _run_git(self, *args):
        args = ['git'] + list(args)
        ret = subprocess.call(args)
        if ret:
            print('ERROR git command %s returned %s' %
                  (args, ret))
            sys.exit(1)

    def _git_push_to_try(self, msg):
        self._run_git('commit', '--allow-empty', '-m', msg)
        try:
            self._run_git('push', 'hg::ssh://hg.mozilla.org/try',
                          '+HEAD:refs/heads/branches/default/tip')
        finally:
            self._run_git('reset', 'HEAD~')

    def _git_find_changed_files(self):
        # This finds the files changed on the current branch based on the
        # diff of the current branch its merge-base base with other branches.
        try:
            args = ['git', 'rev-parse', 'HEAD']
            current_branch = subprocess.check_output(args).strip()
            args = ['git', 'for-each-ref', 'refs/heads', 'refs/remotes',
                    '--format=%(objectname)']
            all_branches = subprocess.check_output(args).splitlines()
            other_branches = set(all_branches) - set([current_branch])
            args = ['git', 'merge-base', 'HEAD'] + list(other_branches)
            base_commit = subprocess.check_output(args).strip()
            args = ['git', 'diff', '--name-only', '-z', 'HEAD', base_commit]
            return subprocess.check_output(args).strip('\0').split('\0')
        except subprocess.CalledProcessError as e:
            print('Failed while determining files changed on this branch')
            print('Failed whle running: %s' % args)
            print(e.output)
            sys.exit(1)

    def _hg_find_changed_files(self):
        hg_args = [
            'hg', 'log', '-r',
            '::. and not public()',
            '--template',
            '{join(files, "\n")}\n',
        ]
        try:
            return subprocess.check_output(hg_args).splitlines()
        except subprocess.CalledProcessError as e:
            print('Failed while finding files changed since the last '
                  'public ancestor')
            print('Failed whle running: %s' % hg_args)
            print(e.output)
            sys.exit(1)

    def find_changed_files(self):
        """Finds files changed in a local source tree.

        For hg, changes since the last public ancestor of '.' are
        considered. For git, changes in the current branch are considered.
        """
        if self._use_git:
            return self._git_find_changed_files()
        return self._hg_find_changed_files()

    def push_to_try(self, msg, verbose):
        if not self._use_git:
            try:
                hg_args = ['hg', 'push-to-try', '-m', msg]
                subprocess.check_call(hg_args, stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                print('ERROR hg command %s returned %s' % (hg_args, e.returncode))
                print('\nmach failed to push to try. There may be a problem '
                      'with your ssh key, or another issue with your mercurial '
                      'installation.')
                # Check for the presence of the "push-to-try" extension, and
                # provide instructions if it can't be found.
                try:
                    subprocess.check_output(['hg', 'showconfig',
                                             'extensions.push-to-try'])
                except subprocess.CalledProcessError:
                    print('\nThe "push-to-try" hg extension is required. It '
                          'can be installed to Mercurial 3.3 or above by '
                          'running ./mach mercurial-setup')
                sys.exit(1)
        else:
            try:
                which.which('git-cinnabar')
                self._git_push_to_try(msg)
            except which.WhichError:
                print('ERROR git-cinnabar is required to push from git to try with'
                      'the autotry command.\n\nMore information can by found at '
                      'https://github.com/glandium/git-cinnabar')
                sys.exit(1)

    def find_uncommited_changes(self):
        if self._use_git:
            stat = subprocess.check_output(['git', 'status', '-z'])
            return any(len(entry.strip()) and entry.strip()[0] in ('A', 'M', 'D')
                       for entry in stat.split('\0'))
        else:
            stat = subprocess.check_output(['hg', 'status'])
            return any(len(entry.strip()) and entry.strip()[0] in ('A', 'M', 'R')
                       for entry in stat.splitlines())

    def find_paths_and_tags(self, verbose):
        paths, tags = set(), set()
        changed_files = self.find_changed_files()
        if changed_files:
            if verbose:
                print("Pushing tests based on modifications to the "
                      "following files:\n\t%s" % "\n\t".join(changed_files))

            from mozbuild.frontend.reader import (
                BuildReader,
                EmptyConfig,
            )

            config = EmptyConfig(self.topsrcdir)
            reader = BuildReader(config)
            files_info = reader.files_info(changed_files)

            for path, info in files_info.items():
                paths |= info.test_files
                tags |= info.test_tags

            if verbose:
                if paths:
                    print("Pushing tests based on the following patterns:\n\t%s" %
                          "\n\t".join(paths))
                if tags:
                    print("Pushing tests based on the following tags:\n\t%s" %
                          "\n\t".join(tags))
        return paths, tags
