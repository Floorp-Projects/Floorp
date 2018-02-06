# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import signal
import sys
import traceback
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor
from math import ceil
from multiprocessing import cpu_count
from subprocess import CalledProcessError

from mozversioncontrol import get_repository_object, MissingUpstreamRepo, InvalidRepoPath

from .errors import LintersNotConfigured
from .parser import Parser
from .pathutils import findobject
from .types import supported_types


def _run_linters(config, paths, **lintargs):
    results = defaultdict(list)
    failed = []

    func = supported_types[config['type']]
    res = func(paths, config, **lintargs) or []

    if not isinstance(res, (list, tuple)):
        if res:
            failed.append(config['name'])
    else:
        for r in res:
            results[r.path].append(r)
    return results, failed


def _run_worker(*args, **kwargs):
    try:
        return _run_linters(*args, **kwargs)
    except Exception:
        # multiprocessing seems to munge worker exceptions, print
        # it here so it isn't lost.
        traceback.print_exc()
        raise
    finally:
        sys.stdout.flush()


class LintRoller(object):
    """Registers and runs linters.

    :param root: Path to which relative paths will be joined. If
                 unspecified, root will either be determined from
                 version control or cwd.
    :param lintargs: Arguments to pass to the underlying linter(s).
    """
    MAX_PATHS_PER_JOB = 50  # set a max size to prevent command lines that are too long on Windows

    def __init__(self, root, **lintargs):
        self.parse = Parser()
        try:
            self.vcs = get_repository_object(root)
        except InvalidRepoPath:
            self.vcs = None

        self.linters = []
        self.lintargs = lintargs
        self.lintargs['root'] = root

        # linters that return non-zero
        self.failed = set()
        self.root = root

    def read(self, paths):
        """Parse one or more linters and add them to the registry.

        :param paths: A path or iterable of paths to linter definitions.
        """
        if isinstance(paths, basestring):
            paths = (paths,)

        for path in paths:
            self.linters.extend(self.parse(path))

    def setup(self):
        """Run setup for applicable linters"""
        if not self.linters:
            raise LintersNotConfigured

        failed = set()
        for linter in self.linters:
            if 'setup' not in linter:
                continue

            try:
                res = findobject(linter['setup'])(self.root)
            except Exception:
                traceback.print_exc()
                res = 1

            if res:
                failed.add(linter['name'])

        if failed:
            print("error: problem with lint setup, skipping {}".format(', '.join(sorted(failed))))
            self.linters = [l for l in self.linters if l['name'] not in failed]
            self.failed.update(failed)
            return 1
        return 0

    def _generate_jobs(self, paths, num_procs):
        """A job is of the form (<linter:dict>, <paths:list>)."""
        chunk_size = min(self.MAX_PATHS_PER_JOB, int(ceil(float(len(paths)) / num_procs)))
        while paths:
            for linter in self.linters:
                yield linter, paths[:chunk_size]
            paths = paths[chunk_size:]

    def roll(self, paths=None, outgoing=None, workdir=None, num_procs=None):
        """Run all of the registered linters against the specified file paths.

        :param paths: An iterable of files and/or directories to lint.
        :param outgoing: Lint files touched by commits that are not on the remote repository.
        :param workdir: Lint all files touched in the working directory.
        :param num_procs: The number of processes to use. Default: cpu count
        :return: A dictionary with file names as the key, and a list of
                 :class:`~result.ResultContainer`s as the value.
        """
        if not self.linters:
            raise LintersNotConfigured

        # Need to use a set in case vcs operations specify the same file
        # more than once.
        paths = paths or set()
        if isinstance(paths, basestring):
            paths = set([paths])
        elif isinstance(paths, (list, tuple)):
            paths = set(paths)

        if not self.vcs and (workdir or outgoing):
            print("error: '{}' is not a known repository, can't use "
                  "--workdir or --outgoing".format(self.lintargs['root']))

        # Calculate files from VCS
        try:
            if workdir:
                paths.update(self.vcs.get_changed_files('AM', mode=workdir))
            if outgoing:
                try:
                    paths.update(self.vcs.get_outgoing_files('AM', upstream=outgoing))
                except MissingUpstreamRepo:
                    print("warning: could not find default push, specify a remote for --outgoing")
        except CalledProcessError as e:
            print("error running: {}".format(' '.join(e.cmd)))
            if e.output:
                print(e.output)

        if not paths and (workdir or outgoing):
            print("warning: no files linted")
            return {}

        paths = paths or ['.']

        # This will convert paths back to a list, but that's ok since
        # we're done adding to it.
        paths = map(os.path.abspath, paths)

        num_procs = num_procs or cpu_count()
        all_results = defaultdict(list)
        with ProcessPoolExecutor(num_procs) as executor:
            futures = [executor.submit(_run_worker, config, p, **self.lintargs)
                       for config, p in self._generate_jobs(paths, num_procs)]
            # ignore SIGINT in parent so we can still get partial results
            # from child processes. These should shutdown quickly anyway.
            orig_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)
            for future in futures:
                results, failed = future.result()
                if failed:
                    self.failed.update(set(failed))
                for k, v in results.iteritems():
                    all_results[k].extend(v)

        signal.signal(signal.SIGINT, orig_sigint)
        return all_results
