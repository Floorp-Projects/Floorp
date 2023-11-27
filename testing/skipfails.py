# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import pprint
import urllib.parse
from enum import Enum

import bugzilla
import mozci.push


class Classification(object):
    "Classification of the failure (not the task result)"

    UNKNOWN = "unknown"
    INTERMITTENT = "intermittent"
    DISABLE_RECOMMENDED = "disable_recommended"
    SECONDARY = "secondary"


class Run(Enum):
    """
    constant indexes for attributes of a run
    """

    MANIFEST = 0
    TASK_ID = 1
    TASK_LABEL = 2
    RESULT = 3
    CLASSIFICATION = 4


class Skipfails(object):
    "mach manifest skip-fails implementation: Update manifests to skip failing tests"

    REPO = "repo"
    REVISION = "revision"
    TREEHERDER = "treeherder.mozilla.org"
    BUGZILLA_SERVER_DEFAULT = "bugzilla.allizom.org"

    def __init__(
        self,
        command_context=None,
        try_url="",
        verbose=False,
        bugzilla=None,
        dry_run=False,
    ):
        self.command_context = command_context
        if isinstance(try_url, list) and len(try_url) == 1:
            self.try_url = try_url[0]
        else:
            self.try_url = try_url
        self.dry_run = dry_run
        self.verbose = verbose
        if bugzilla is not None:
            self.bugzilla = bugzilla
        else:
            if "BUGZILLA" in os.environ:
                self.bugzilla = os.environ["BUGZILLA"]
            else:
                self.bugzilla = Skipfails.BUGZILLA_SERVER_DEFAULT
        self.component = "skip-fails"
        self._bzapi = None

    def _initialize_bzapi(self):
        """Lazily initializes the Bugzilla API"""
        if self._bzapi is None:
            self._bzapi = bugzilla.Bugzilla(self.bugzilla)

    def error(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.ERROR, self.component, {"error": str(e)}, "ERROR: {error}"
            )
        else:
            print(f"ERROR: {e}")

    def warning(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.WARNING, self.component, {"error": str(e)}, "WARNING: {error}"
            )
        else:
            print(f"WARNING: {e}")

    def info(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.INFO, self.component, {"error": str(e)}, "INFO: {error}"
            )
        else:
            print(f"INFO: {e}")

    def pprint(self, obj):
        pp = pprint.PrettyPrinter(indent=4)
        pp.pprint(obj)

    def run(self):
        "Run skip-fails on try_url, return True on success"

        revision, repo = self.get_revision(self.try_url)
        tasks = self.get_tasks(revision, repo)
        failures = self.get_failures(tasks)
        self.error("skip-fails not implemented yet")
        if self.verbose:
            self.info(f"bugzilla instance: {self.bugzilla}")
            self.info(f"dry_run: {self.dry_run}")
            self.pprint(failures)
        return False

    def get_revision(self, url):
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme != "https":
            raise ValueError("try_url scheme not https")
        if parsed.netloc != Skipfails.TREEHERDER:
            raise ValueError(f"try_url server not {Skipfails.TREEHERDER}")
        if len(parsed.query) == 0:
            raise ValueError("try_url query missing")
        query = urllib.parse.parse_qs(parsed.query)
        if Skipfails.REVISION not in query:
            raise ValueError("try_url query missing revision")
        revision = query[Skipfails.REVISION][0]
        if Skipfails.REPO in query:
            repo = query[Skipfails.REPO][0]
        else:
            repo = "try"
        if self.verbose:
            self.info(f"considering {repo} revision={revision}")
        return revision, repo

    def get_tasks(self, revision, repo):
        push = mozci.push.Push(revision, repo)
        return push.tasks

    def get_failures(self, tasks):
        """
        find failures and create structure comprised of runs by path:
           {path: [[manifest, task_id, task_label, result, classification], ...]}
           result:
            * False (failed)
            * True (pased)
           classification: Classification
            * unknown (default)
            * intermittent (not enough failures)
            * disable_recommended (enough repeated failures)
            * secondary (not first failure in group)
        """

        runsby = {}  # test runs indexed by path
        for task in tasks:
            try:
                for manifest in task.failure_types:
                    for test in task.failure_types[manifest]:
                        path = test[0]
                        if path not in runsby:
                            runsby[path] = []  # create runs list
                        # reduce duplicate runs in the same task
                        if [manifest, task.id] not in runsby[path]:
                            runsby[path].append(
                                [
                                    manifest,
                                    task.id,
                                    task.label,
                                    False,
                                    Classification.UNKNOWN,
                                ]
                            )
            except AttributeError as ae:
                self.warning(f"unknown attribute in task: {ae}")

        # now collect all results, even if no failure
        paths = runsby.keys()
        for path in paths:
            runs = runsby[path]
            for index in range(len(runs)):
                manifest, id, label, result, classification = runs[index]
                for task in tasks:
                    if label == task.label:
                        for result in [r for r in task.results if r.group == manifest]:
                            # add result to runsby
                            if task.id not in [
                                run[Run.TASK_ID.value] for run in runsby[path]
                            ]:
                                runsby[path].append(
                                    [
                                        manifest,
                                        task.id,
                                        label,
                                        result.ok,
                                        Classification.UNKNOWN,
                                    ]
                                )
                            else:
                                runsby[path][index][Run.RESULT.value] = result.ok

        # sort by first failure in directory and classify others as secondary
        for path in runsby:
            # if group and label are the same, get all paths
            paths = [
                p
                for p in runsby
                if runsby[p][0][Run.MANIFEST.value]
                == runsby[path][0][Run.MANIFEST.value]
                and runsby[p][0][Run.TASK_LABEL.value]
                == runsby[path][0][Run.TASK_LABEL.value]
            ]
            paths.sort()
            for secondary_path in paths[1:]:
                runs = runsby[secondary_path]
                for index in range(len(runs)):
                    runs[index][Run.CLASSIFICATION.value] = Classification.SECONDARY

        # now print out final results
        failures = []
        for path in runsby:
            runs = runsby[path]
            total_runs = len(runs)
            failed_runs = len([run for run in runs if run[Run.RESULT.value] is False])
            classification = runs[0][Run.CLASSIFICATION.value]
            if total_runs >= 3 and classification != Classification.SECONDARY:
                if failed_runs / total_runs >= 0.5:
                    classification = Classification.DISABLE_RECOMMENDED
                else:
                    classification = Classification.INTERMITTENT
            failure = {}
            failure["path"] = path
            failure["manifest"] = runs[0][Run.MANIFEST.value]
            failure["failures"] = failed_runs
            failure["totalruns"] = total_runs
            failure["classification"] = classification
            failure["label"] = runs[0][Run.TASK_LABEL.value]
            failures.append(failure)
        return failures

    def get_bug(self, bug):
        """Get bug by bug number"""

        self._initialize_bzapi()
        bug = self._bzapi.getbug(bug)
        return bug
