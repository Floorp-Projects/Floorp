# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import atexit
import copy
import logging
import os
import signal
import sys
import time
import traceback
from concurrent.futures import ProcessPoolExecutor
from concurrent.futures.process import _python_exit as futures_atexit
from itertools import chain
from math import ceil
from multiprocessing import cpu_count, get_context
from multiprocessing.queues import Queue
from subprocess import CalledProcessError

import mozpack.path as mozpath
from mozversioncontrol import (
    get_repository_object,
    MissingUpstreamRepo,
    InvalidRepoPath,
)

from .errors import LintersNotConfigured, NoValidLinter
from .parser import Parser
from .pathutils import findobject
from .result import ResultSummary
from .types import supported_types

SHUTDOWN = False
orig_sigint = signal.getsignal(signal.SIGINT)

logger = logging.getLogger("mozlint")
handler = logging.StreamHandler()
formatter = logging.Formatter(
    "%(asctime)s.%(msecs)d %(lintname)s (%(pid)s) | %(message)s", "%H:%M:%S"
)
handler.setFormatter(formatter)
logger.addHandler(handler)


def _run_worker(config, paths, **lintargs):
    log = logging.LoggerAdapter(
        logger, {"lintname": config.get("name"), "pid": os.getpid()}
    )
    lintargs["log"] = log
    result = ResultSummary(lintargs["root"])

    if SHUTDOWN:
        return result

    # Override warnings setup for code review
    # Only activating when code_review_warnings is set on a linter.yml in use
    if os.environ.get("CODE_REVIEW") == "1" and config.get("code_review_warnings"):
        lintargs["show_warnings"] = True

    func = supported_types[config["type"]]
    start_time = time.time()
    try:
        res = func(paths, config, **lintargs)
        # Some linters support fixed operations
        # dict returned - {"results":results,"fixed":fixed}
        if isinstance(res, dict):
            result.fixed += res["fixed"]
            res = res["results"] or []
        elif isinstance(res, list):
            res = res or []
        else:
            print("Unexpected type received")
            assert False
    except Exception:
        traceback.print_exc()
        res = 1
    except (KeyboardInterrupt, SystemExit):
        return result
    finally:
        end_time = time.time()
        log.debug("Finished in {:.2f} seconds".format(end_time - start_time))
        sys.stdout.flush()

    if not isinstance(res, (list, tuple)):
        if res:
            result.failed_run.add(config["name"])
    else:
        for r in res:
            if not lintargs.get("show_warnings") and r.level == "warning":
                result.suppressed_warnings[r.path] += 1
                continue

            result.issues[r.path].append(r)

    return result


class InterruptableQueue(Queue):
    """A multiprocessing.Queue that catches KeyboardInterrupt when a worker is
    blocking on it and returns None.

    This is needed to gracefully handle KeyboardInterrupts when a worker is
    blocking on ProcessPoolExecutor's call queue.
    """

    def __init__(self, *args, **kwargs):
        kwargs["ctx"] = get_context()
        super(InterruptableQueue, self).__init__(*args, **kwargs)

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


def wrap_futures_atexit():
    """Sometimes futures' atexit handler can spew tracebacks. This wrapper
    suppresses them."""
    try:
        futures_atexit()
    except Exception:
        # Generally `atexit` handlers aren't supposed to raise exceptions, but the
        # futures' handler can sometimes raise when the user presses `CTRL-C`. We
        # suppress all possible exceptions here so users have a nice experience
        # when canceling their lint run. Any exceptions raised by this function
        # won't be useful anyway.
        pass


atexit.unregister(futures_atexit)
atexit.register(wrap_futures_atexit)


class LintRoller(object):
    """Registers and runs linters.

    :param root: Path to which relative paths will be joined. If
                 unspecified, root will either be determined from
                 version control or cwd.
    :param lintargs: Arguments to pass to the underlying linter(s).
    """

    MAX_PATHS_PER_JOB = (
        50  # set a max size to prevent command lines that are too long on Windows
    )

    def __init__(self, root, exclude=None, **lintargs):
        self.parse = Parser(root)
        try:
            self.vcs = get_repository_object(root)
        except InvalidRepoPath:
            self.vcs = None

        self.linters = []
        self.lintargs = lintargs
        self.lintargs["root"] = root

        # result state
        self.result = ResultSummary(root)

        self.root = root
        self.exclude = exclude or []

        if lintargs.get("show_verbose"):
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.WARNING)

        self.log = logging.LoggerAdapter(
            logger, {"lintname": "mozlint", "pid": os.getpid()}
        )

    def read(self, paths):
        """Parse one or more linters and add them to the registry.

        :param paths: A path or iterable of paths to linter definitions.
        """
        if isinstance(paths, str):
            paths = (paths,)

        for linter in chain(*[self.parse(p) for p in paths]):
            # Add only the excludes present in paths
            linter["local_exclude"] = linter.get("exclude", [])[:]
            # Add in our global excludes
            linter.setdefault("exclude", []).extend(self.exclude)
            self.linters.append(linter)

    def setup(self, virtualenv_manager=None):
        """Run setup for applicable linters"""
        if not self.linters:
            raise NoValidLinter

        for linter in self.linters:
            if "setup" not in linter:
                continue

            try:
                setupargs = copy.deepcopy(self.lintargs)
                setupargs["name"] = linter["name"]
                if virtualenv_manager is not None:
                    setupargs["virtualenv_manager"] = virtualenv_manager
                start_time = time.time()
                res = findobject(linter["setup"])(**setupargs)
                self.log.debug(
                    f"setup for {linter['name']} finished in "
                    f"{round(time.time() - start_time, 2)} seconds"
                )
            except Exception:
                traceback.print_exc()
                res = 1

            if res:
                self.result.failed_setup.add(linter["name"])

        if self.result.failed_setup:
            print(
                "error: problem with lint setup, skipping {}".format(
                    ", ".join(sorted(self.result.failed_setup))
                )
            )
            self.linters = [
                l for l in self.linters if l["name"] not in self.result.failed_setup
            ]
            return 1
        return 0

    def _generate_jobs(self, paths, vcs_paths, num_procs):
        def __get_current_paths(path=self.root):
            return [os.path.join(path, p) for p in os.listdir(path)]

        """A job is of the form (<linter:dict>, <paths:list>)."""
        for linter in self.linters:
            if any(
                os.path.isfile(p) and mozpath.match(p, pattern)
                for pattern in linter.get("support-files", [])
                for p in vcs_paths
            ):
                lpaths = __get_current_paths()
                print(
                    "warning: {} support-file modified, linting entire tree "
                    "(press ctrl-c to cancel)".format(linter["name"])
                )
            else:
                lpaths = paths.union(vcs_paths)

            lpaths = list(lpaths) or __get_current_paths(os.getcwd())
            chunk_size = (
                min(self.MAX_PATHS_PER_JOB, int(ceil(len(lpaths) / num_procs))) or 1
            )
            if linter["type"] == "global":
                # Global linters lint the entire tree in one job.
                chunk_size = len(lpaths) or 1
            assert chunk_size > 0

            while lpaths:
                yield linter, lpaths[:chunk_size]
                lpaths = lpaths[chunk_size:]

    def _collect_results(self, future):
        if future.cancelled():
            return

        # Merge this job's results with our global ones.
        self.result.update(future.result())

    def roll(self, paths=None, outgoing=None, workdir=None, rev=None, num_procs=None):
        """Run all of the registered linters against the specified file paths.

        :param paths: An iterable of files and/or directories to lint.
        :param outgoing: Lint files touched by commits that are not on the remote repository.
        :param workdir: Lint all files touched in the working directory.
        :param num_procs: The number of processes to use. Default: cpu count
        :return: A :class:`~result.ResultSummary` instance.
        """
        if not self.linters:
            raise LintersNotConfigured

        self.result.reset()

        # Need to use a set in case vcs operations specify the same file
        # more than once.
        paths = paths or set()
        if isinstance(paths, str):
            paths = set([paths])
        elif isinstance(paths, (list, tuple)):
            paths = set(paths)

        if not self.vcs and (workdir or outgoing):
            print(
                "error: '{}' is not a known repository, can't use "
                "--workdir or --outgoing".format(self.lintargs["root"])
            )

        # Calculate files from VCS
        vcs_paths = set()
        try:
            if workdir:
                vcs_paths.update(self.vcs.get_changed_files("AM", mode=workdir))
            if rev:
                vcs_paths.update(self.vcs.get_changed_files("AM", rev=rev))
            if outgoing:
                try:
                    vcs_paths.update(
                        self.vcs.get_outgoing_files("AM", upstream=outgoing)
                    )
                except MissingUpstreamRepo:
                    print(
                        "warning: could not find default push, specify a remote for --outgoing"
                    )
        except CalledProcessError as e:
            print("error running: {}".format(" ".join(e.cmd)))
            if e.output:
                print(e.output)

        if not (paths or vcs_paths) and (workdir or outgoing):
            print("warning: no files linted")
            return self.result

        # Make sure all paths are absolute. Join `paths` to cwd and `vcs_paths` to root.
        paths = set(map(os.path.abspath, paths))
        vcs_paths = set(
            [
                os.path.join(self.root, p) if not os.path.isabs(p) else p
                for p in vcs_paths
            ]
        )

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
            executor.shutdown(wait=True)
            print("\nwarning: not all files were linted")
            signal.signal(signal.SIGINT, signal.SIG_IGN)

        signal.signal(signal.SIGINT, _parent_sigint_handler)
        executor.shutdown()
        signal.signal(signal.SIGINT, orig_sigint)
        return self.result
