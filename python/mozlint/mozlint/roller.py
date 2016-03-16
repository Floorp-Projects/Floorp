# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import signal
import traceback
from collections import defaultdict
from Queue import Empty
from multiprocessing import (
    Manager,
    Pool,
    cpu_count,
)

from .errors import LintersNotConfigured
from .types import supported_types
from .parser import Parser


def _run_linters(queue, paths, **lintargs):
    parse = Parser()
    results = defaultdict(list)

    while True:
        try:
            linter_path = queue.get(False)
        except Empty:
            return results

        # Ideally we would pass the entire LINTER definition as an argument
        # to the worker instead of re-parsing it. But passing a function from
        # a dynamically created module (with imp) does not seem to be possible
        # with multiprocessing on Windows.
        linter = parse(linter_path)
        func = supported_types[linter['type']]
        res = func(paths, linter, **lintargs) or []

        for r in res:
            results[r.path].append(r)


def _run_worker(*args, **lintargs):
    try:
        return _run_linters(*args, **lintargs)
    except:
        traceback.print_exc()
        raise


class LintRoller(object):
    """Registers and runs linters.

    :param lintargs: Arguments to pass to the underlying linter(s).
    """

    def __init__(self, **lintargs):
        self.parse = Parser()
        self.linters = []
        self.lintargs = lintargs

    def read(self, paths):
        """Parse one or more linters and add them to the registry.

        :param paths: A path or iterable of paths to linter definitions.
        """
        if isinstance(paths, basestring):
            paths = (paths,)

        for path in paths:
            self.linters.append(self.parse(path))

    def roll(self, paths, num_procs=None):
        """Run all of the registered linters against the specified file paths.

        :param paths: An iterable of files and/or directories to lint.
        :param num_procs: The number of processes to use. Default: cpu count
        :return: A dictionary with file names as the key, and a list of
                 :class:`~result.ResultContainer`s as the value.
        """
        if not self.linters:
            raise LintersNotConfigured

        if isinstance(paths, basestring):
            paths = [paths]

        m = Manager()
        queue = m.Queue()

        for linter in self.linters:
            queue.put(linter['path'])

        num_procs = num_procs or cpu_count()
        num_procs = min(num_procs, len(self.linters))

        # ensure child processes ignore SIGINT so it reaches parent
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        pool = Pool(num_procs)
        signal.signal(signal.SIGINT, orig)

        all_results = defaultdict(list)
        results = []
        for i in range(num_procs):
            results.append(
                pool.apply_async(_run_worker, args=(queue, paths), kwds=self.lintargs))

        for res in results:
            # parent process blocks on res.get()
            for k, v in res.get().iteritems():
                all_results[k].extend(v)

        return all_results
