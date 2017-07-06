# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import signal
import sys
import traceback
from collections import defaultdict
from multiprocessing import Manager, Pool, cpu_count
from Queue import Empty

from .errors import LintersNotConfigured
from .parser import Parser
from .types import supported_types
from .vcs import VCSHelper


def _run_linters(queue, paths, **lintargs):
    results = defaultdict(list)
    failed = []

    while True:
        try:
            # The astute reader may wonder what is preventing the worker from
            # grabbing the next linter config from the queue after a SIGINT.
            # Because this is a Manager.Queue(), it is itself in a child process
            # which also received SIGINT. By the time the worker gets back here,
            # the Queue is dead and IOError is raised.
            config = queue.get(False)
        except (Empty, IOError):
            return results, failed

        func = supported_types[config['type']]
        res = func(paths, config, **lintargs) or []

        if not isinstance(res, (list, tuple)):
            if res:
                failed.append(config['name'])
            continue

        for r in res:
            results[r.path].append(r)


def _run_worker(*args, **lintargs):
    try:
        return _run_linters(*args, **lintargs)
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

    def __init__(self, root=None, **lintargs):
        self.parse = Parser()
        self.vcs = VCSHelper.create()

        self.linters = []
        self.lintargs = lintargs
        self.lintargs['root'] = root or self.vcs.root or os.getcwd()

        # linters that return non-zero
        self.failed = None

    def read(self, paths):
        """Parse one or more linters and add them to the registry.

        :param paths: A path or iterable of paths to linter definitions.
        """
        if isinstance(paths, basestring):
            paths = (paths,)

        for path in paths:
            self.linters.extend(self.parse(path))

    def roll(self, paths=None, outgoing=None, workdir=None, num_procs=None):
        """Run all of the registered linters against the specified file paths.

        :param paths: An iterable of files and/or directories to lint.
        :param outgoing: Lint files touched by commits that are not on the remote repository.
        :param workdir: Lint all files touched in the working directory.
        :param num_procs: The number of processes to use. Default: cpu count
        :return: A dictionary with file names as the key, and a list of
                 :class:`~result.ResultContainer`s as the value.
        """
        # Need to use a set in case vcs operations specify the same file
        # more than once.
        paths = paths or set()
        if isinstance(paths, basestring):
            paths = set([paths])
        elif isinstance(paths, (list, tuple)):
            paths = set(paths)

        if not self.linters:
            raise LintersNotConfigured

        # Calculate files from VCS
        if workdir:
            paths.update(self.vcs.by_workdir(workdir))
        if outgoing:
            paths.update(self.vcs.by_outgoing(outgoing))

        if not paths and (workdir or outgoing):
            print("warning: no files linted")
            return {}

        paths = paths or ['.']

        # This will convert paths back to a list, but that's ok since
        # we're done adding to it.
        paths = map(os.path.abspath, paths)

        # Set up multiprocessing
        m = Manager()
        queue = m.Queue()

        for config in self.linters:
            queue.put(config)

        num_procs = num_procs or cpu_count()
        num_procs = min(num_procs, len(self.linters))
        pool = Pool(num_procs)

        all_results = defaultdict(list)
        workers = []
        for i in range(num_procs):
            workers.append(
                pool.apply_async(_run_worker, args=(queue, paths), kwds=self.lintargs))
        pool.close()

        # ignore SIGINT in parent so we can still get partial results
        # from child processes. These should shutdown quickly anyway.
        orig_sigint = signal.signal(signal.SIGINT, signal.SIG_IGN)
        self.failed = []
        for worker in workers:
            # parent process blocks on worker.get()
            results, failed = worker.get()
            if failed:
                self.failed.extend(failed)
            for k, v in results.iteritems():
                all_results[k].extend(v)

        signal.signal(signal.SIGINT, orig_sigint)
        m.shutdown()
        return all_results
