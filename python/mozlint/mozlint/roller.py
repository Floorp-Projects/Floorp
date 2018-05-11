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
from multiprocessing.queues import Queue
from subprocess import CalledProcessError

import mozpack.path as mozpath
from mozversioncontrol import get_repository_object, MissingUpstreamRepo, InvalidRepoPath

from .errors import LintersNotConfigured
from .parser import Parser
from .pathutils import findobject
from .types import supported_types

SHUTDOWN = False
orig_sigint = signal.getsignal(signal.SIGINT)


def _run_worker(config, paths, **lintargs):
    results = defaultdict(list)
    failed = []

    if SHUTDOWN:
        return results, failed

    func = supported_types[config['type']]
    try:
        res = func(paths, config, **lintargs) or []
    except Exception:
        traceback.print_exc()
        res = 1
    except (KeyboardInterrupt, SystemExit):
        return results, failed
    finally:
        sys.stdout.flush()

    if not isinstance(res, (list, tuple)):
        if res:
            failed.append(config['name'])
    else:
        for r in res:
            results[r.path].append(r)
    return results, failed


class InterruptableQueue(Queue):
    """A multiprocessing.Queue that catches KeyboardInterrupt when a worker is
    blocking on it and returns None.

    This is needed to gracefully handle KeyboardInterrupts when a worker is
    blocking on ProcessPoolExecutor's call queue.
    """

    def get(self, *args, **kwargs):
        try:
            return Queue.get(self, *args, **kwargs)
        except KeyboardInterrupt:
            return None


def _worker_sigint_handler(signum, frame):
    """Sigint handler for the worker subprocesses.

    Tells workers not to process the extra jobs on the call queue that couldn't
    be canceled by the parent process.
    """
    global SHUTDOWN
    SHUTDOWN = True
    orig_sigint(signum, frame)


class LintRoller(object):
    """Registers and runs linters.

    :param root: Path to which relative paths will be joined. If
                 unspecified, root will either be determined from
                 version control or cwd.
    :param lintargs: Arguments to pass to the underlying linter(s).
    """
    MAX_PATHS_PER_JOB = 50  # set a max size to prevent command lines that are too long on Windows

    def __init__(self, root, **lintargs):
        self.parse = Parser(root)
        try:
            self.vcs = get_repository_object(root)
        except InvalidRepoPath:
            self.vcs = None

        self.linters = []
        self.lintargs = lintargs
        self.lintargs['root'] = root

        # result state
        self.failed = None
        self.failed_setup = None
        self.results = None

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

        self.failed_setup = set()
        for linter in self.linters:
            if 'setup' not in linter:
                continue

            try:
                res = findobject(linter['setup'])(self.root)
            except Exception:
                traceback.print_exc()
                res = 1

            if res:
                self.failed_setup.add(linter['name'])

        if self.failed_setup:
            print("error: problem with lint setup, skipping {}".format(
                    ', '.join(sorted(self.failed_setup))))
            self.linters = [l for l in self.linters if l['name'] not in self.failed_setup]
            return 1
        return 0

    def _generate_jobs(self, paths, vcs_paths, num_procs):
        """A job is of the form (<linter:dict>, <paths:list>)."""
        for linter in self.linters:
            if any(os.path.isfile(p) and mozpath.match(p, pattern)
                    for pattern in linter.get('support-files', []) for p in vcs_paths):
                lpaths = [self.root]
                print("warning: {} support-file modified, linting entire tree "
                      "(press ctrl-c to cancel)".format(linter['name']))
            else:
                lpaths = paths.union(vcs_paths)

            lpaths = list(lpaths) or [os.getcwd()]
            chunk_size = min(self.MAX_PATHS_PER_JOB, int(ceil(len(lpaths) / num_procs))) or 1
            assert chunk_size > 0

            while lpaths:
                yield linter, lpaths[:chunk_size]
                lpaths = lpaths[chunk_size:]

    def _collect_results(self, future):
        if future.cancelled():
            return

        results, failed = future.result()
        if failed:
            self.failed.update(set(failed))
        for k, v in results.iteritems():
            self.results[k].extend(v)

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

        # reset result state
        self.results = defaultdict(list)
        self.failed = set()

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
        vcs_paths = set()
        try:
            if workdir:
                vcs_paths.update(self.vcs.get_changed_files('AM', mode=workdir))
            if outgoing:
                try:
                    vcs_paths.update(self.vcs.get_outgoing_files('AM', upstream=outgoing))
                except MissingUpstreamRepo:
                    print("warning: could not find default push, specify a remote for --outgoing")
        except CalledProcessError as e:
            print("error running: {}".format(' '.join(e.cmd)))
            if e.output:
                print(e.output)

        if not (paths or vcs_paths) and (workdir or outgoing):
            print("warning: no files linted")
            return {}

        # Make sure all paths are absolute. Join `paths` to cwd and `vcs_paths` to root.
        paths = set(map(os.path.abspath, paths))
        vcs_paths = set([os.path.join(self.root, p) if not os.path.isabs(p) else p
                         for p in vcs_paths])

        num_procs = num_procs or cpu_count()
        jobs = list(self._generate_jobs(paths, vcs_paths, num_procs))

        # Make sure we never spawn more processes than we have jobs.
        num_procs = min(len(jobs), num_procs) or 1

        signal.signal(signal.SIGINT, _worker_sigint_handler)
        executor = ProcessPoolExecutor(num_procs)
        executor._call_queue = InterruptableQueue(executor._call_queue._maxsize)

        # Submit jobs to the worker pool. The _collect_results method will be
        # called when a job is finished. We store the futures so that they can
        # be canceled in the event of a KeyboardInterrupt.
        futures = []
        for job in jobs:
            future = executor.submit(_run_worker, *job, **self.lintargs)
            future.add_done_callback(self._collect_results)
            futures.append(future)

        def _parent_sigint_handler(signum, frame):
            """Sigint handler for the parent process.

            Cancels all jobs that have not yet been placed on the call queue.
            The parent process won't exit until all workers have terminated.
            Assuming the linters are implemented properly, this shouldn't take
            more than a couple seconds.
            """
            [f.cancel() for f in futures]
            executor.shutdown(wait=False)
            print("\nwarning: not all files were linted")
            signal.signal(signal.SIGINT, signal.SIG_IGN)

        signal.signal(signal.SIGINT, _parent_sigint_handler)
        executor.shutdown()
        signal.signal(signal.SIGINT, orig_sigint)
        return self.results
