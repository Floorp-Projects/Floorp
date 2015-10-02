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
    parser.add_argument('-b', dest='builds', default='do',
                        help='Build types to run (d for debug, o for optimized).')
    parser.add_argument('-p', dest='platforms', action="append",
                        help='Platforms to run (required if not found in the environment as AUTOTRY_PLATFORM_HINT).')
    parser.add_argument('-u', dest='tests', action="append",
                        help='Test suites to run in their entirety.')
    parser.add_argument('-t', dest="talos", action="append",
                        help='Talos suites to run.')
    parser.add_argument('--tag', dest='tags', action='append',
                        help='Restrict tests to the given tag (may be specified multiple times).')
    parser.add_argument('--and', action='store_true', dest="intersection",
                        help='When -u and paths are supplied run only the intersection of the tests specified by the two arguments.')
    parser.add_argument('--no-push', dest='push', action='store_false',
                        help='Do not push to try as a result of running this command (if '
                        'specified this command will only print calculated try '
                        'syntax and selection info).')
    parser.add_argument('--save', dest="save", action='store',
                        help="Save the command line arguments for future use with --preset.")
    parser.add_argument('--preset', dest="load", action='store',
                        help="Load a saved set of arguments. Additional arguments will override saved ones.")
    parser.add_argument('extra_args', nargs=argparse.REMAINDER,
                        help='Extra arguments to put in the try push.')
    parser.add_argument('-v', "--verbose", dest='verbose', action='store_true', default=False,
                        help='Print detailed information about the resulting test selection '
                        'and commands performed.')
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
                           'mochitest-e10s-browser-chrome-1'],
        'devtools-chrome': ['mochitest-dt',
                            'mochitest-e10s-devtools-chrome'],
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

    def __init__(self, topsrcdir, resolver, mach_context):
        self.topsrcdir = topsrcdir
        self.resolver = resolver
        self.mach_context = mach_context

        if os.path.exists(os.path.join(self.topsrcdir, '.hg')):
            self._use_git = False
        else:
            self._use_git = True

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

    def calc_try_syntax(self, platforms, tests, talos, builds, paths_by_flavor, tags,
                        extra_args, intersection):
        parts = ["try:", "-b", builds, "-p", ",".join(platforms)]

        suites = tests if not intersection else {}
        paths = set()
        for flavor, flavor_tests in paths_by_flavor.iteritems():
            suite = self.flavor_suites[flavor]
            if suite not in suites and (not intersection or suite in tests):
                for job_name in self.flavor_jobs[flavor]:
                    for test in flavor_tests:
                        paths.add("%s:%s" % (flavor, test))
                    suites[job_name] = tests.get(suite, [])

        if not suites:
            raise ValueError("No tests found matching filters")

        parts.append("-u")
        parts.append(",".join("%s%s" % (k, "[%s]" % ",".join(v) if v else "")
                              for k,v in sorted(suites.items())) if suites else "none")

        parts.append("-t")
        parts.append(",".join(talos) if talos else "none")

        if tags:
            parts.append(' '.join('--tag %s' % t for t in tags))

        if extra_args is not None:
            parts.extend(extra_args)

        if paths:
            parts.append("--try-test-paths %s" % " ".join(sorted(paths)))

        return " ".join(parts)

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
                print('The "push-to-try" hg extension is required to push from '
                      'hg to try with the autotry command.\n\nIt can be installed '
                      'to Mercurial 3.3 or above by running ./mach mercurial-setup')
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
