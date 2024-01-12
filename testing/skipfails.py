# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import gzip
import io
import json
import logging
import os
import os.path
import pprint
import re
import sys
import tempfile
import urllib.parse
from enum import Enum
from pathlib import Path
from xmlrpc.client import Fault

from yaml import load

try:
    from yaml import CLoader as Loader
except ImportError:
    from yaml import Loader

import bugzilla
import mozci.push
import requests
from manifestparser import ManifestParser
from manifestparser.toml import add_skip_if, alphabetize_toml_str, sort_paths
from mozci.task import TestTask
from mozci.util.taskcluster import get_task

BUGZILLA_AUTHENTICATION_HELP = "Must create a Bugzilla API key per https://github.com/mozilla/mozci-tools/blob/main/citools/test_triage_bug_filer.py"
TASK_LOG = "live_backing.log"
TASK_ARTIFACT = "public/logs/" + TASK_LOG
ATTACHMENT_DESCRIPTION = "Compressed " + TASK_ARTIFACT + " for task "
ATTACHMENT_REGEX = (
    r".*Created attachment ([0-9]+)\n.*"
    + ATTACHMENT_DESCRIPTION
    + "([A-Za-z0-9_-]+)\n.*"
)


class MockResult(object):
    def __init__(self, result):
        self.result = result

    @property
    def group(self):
        return self.result["group"]

    @property
    def ok(self):
        _ok = self.result["ok"]
        return _ok


class MockTask(object):
    def __init__(self, task):
        self.task = task
        if "results" in self.task:
            self.task["results"] = [
                MockResult(result) for result in self.task["results"]
            ]
        else:
            self.task["results"] = []

    @property
    def failure_types(self):
        if "failure_types" in self.task:
            return self.task["failure_types"]
        else:  # note no failure_types in Task object
            return {}

    @property
    def id(self):
        return self.task["id"]

    @property
    def label(self):
        return self.task["label"]

    @property
    def results(self):
        return self.task["results"]


class Classification(object):
    "Classification of the failure (not the task result)"

    DISABLE_MANIFEST = "disable_manifest"  # crash found
    DISABLE_RECOMMENDED = "disable_recommended"  # disable first failing path
    INTERMITTENT = "intermittent"
    SECONDARY = "secondary"  # secondary failing path
    SUCCESS = "success"  # path always succeeds
    UNKNOWN = "unknown"


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
        turbo=False,
    ):
        self.command_context = command_context
        if self.command_context is not None:
            self.topsrcdir = self.command_context.topsrcdir
        else:
            self.topsrcdir = Path(__file__).parent.parent
        self.topsrcdir = os.path.normpath(self.topsrcdir)
        if isinstance(try_url, list) and len(try_url) == 1:
            self.try_url = try_url[0]
        else:
            self.try_url = try_url
        self.dry_run = dry_run
        self.verbose = verbose
        self.turbo = turbo
        if bugzilla is not None:
            self.bugzilla = bugzilla
        else:
            if "BUGZILLA" in os.environ:
                self.bugzilla = os.environ["BUGZILLA"]
            else:
                self.bugzilla = Skipfails.BUGZILLA_SERVER_DEFAULT
        self.component = "skip-fails"
        self._bzapi = None
        self._attach_rx = None
        self.variants = {}
        self.tasks = {}
        self.pp = None
        self.headers = {}  # for Treeherder requests
        self.headers["Accept"] = "application/json"
        self.headers["User-Agent"] = "treeherder-pyclient"
        self.jobs_url = "https://treeherder.mozilla.org/api/jobs/"
        self.push_ids = {}
        self.job_ids = {}

    def _initialize_bzapi(self):
        """Lazily initializes the Bugzilla API"""
        if self._bzapi is None:
            self._bzapi = bugzilla.Bugzilla(self.bugzilla)
            self._attach_rx = re.compile(ATTACHMENT_REGEX, flags=re.M)

    def pprint(self, obj):
        if self.pp is None:
            self.pp = pprint.PrettyPrinter(indent=4, stream=sys.stderr)
        self.pp.pprint(obj)
        sys.stderr.flush()

    def error(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.ERROR, self.component, {"error": str(e)}, "ERROR: {error}"
            )
        else:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)

    def warning(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.WARNING, self.component, {"error": str(e)}, "WARNING: {error}"
            )
        else:
            print(f"WARNING: {e}", file=sys.stderr, flush=True)

    def info(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.INFO, self.component, {"error": str(e)}, "INFO: {error}"
            )
        else:
            print(f"INFO: {e}", file=sys.stderr, flush=True)

    def vinfo(self, e):
        if self.verbose:
            self.info(e)

    def run(
        self,
        meta_bug_id=None,
        save_tasks=None,
        use_tasks=None,
        save_failures=None,
        use_failures=None,
        max_failures=-1,
    ):
        "Run skip-fails on try_url, return True on success"

        try_url = self.try_url
        revision, repo = self.get_revision(try_url)

        if use_tasks is not None:
            if os.path.exists(use_tasks):
                self.vinfo(f"use tasks: {use_tasks}")
                tasks = self.read_json(use_tasks)
                tasks = [MockTask(task) for task in tasks]
            else:
                self.error(f"uses tasks JSON file does not exist: {use_tasks}")
                return False
        else:
            tasks = self.get_tasks(revision, repo)

        if use_failures is not None:
            if os.path.exists(use_failures):
                self.vinfo(f"use failures: {use_failures}")
                failures = self.read_json(use_failures)
            else:
                self.error(f"use failures JSON file does not exist: {use_failures}")
                return False
        else:
            failures = self.get_failures(tasks)
            if save_failures is not None:
                self.vinfo(f"save failures: {save_failures}")
                self.write_json(save_failures, failures)

        if save_tasks is not None:
            self.vinfo(f"save tasks: {save_tasks}")
            self.write_tasks(save_tasks, tasks)

        num_failures = 0
        for manifest in failures:
            if not manifest.endswith(".toml"):
                self.warning(f"cannot process skip-fails on INI manifests: {manifest}")
            else:
                for path in failures[manifest]["path"]:
                    for label in failures[manifest]["path"][path]:
                        classification = failures[manifest]["path"][path][label][
                            "classification"
                        ]
                        if classification.startswith("disable_") or (
                            self.turbo and classification == Classification.SECONDARY
                        ):
                            for task_id in failures[manifest]["path"][path][label][
                                "runs"
                            ].keys():
                                self.skip_failure(
                                    manifest,
                                    path,
                                    label,
                                    classification,
                                    task_id,
                                    try_url,
                                    revision,
                                    repo,
                                    meta_bug_id,
                                )
                                num_failures += 1
                                if max_failures >= 0 and num_failures >= max_failures:
                                    self.warning(
                                        f"max_failures={max_failures} threshold reached. stopping."
                                    )
                                    return True
                                break  # just use the first task_id
        return True

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
        self.vinfo(f"considering {repo} revision={revision}")
        return revision, repo

    def get_tasks(self, revision, repo):
        push = mozci.push.Push(revision, repo)
        return push.tasks

    def get_failures(self, tasks):
        """
        find failures and create structure comprised of runs by path:
           result:
            * False (failed)
            * True (passed)
           classification: Classification
            * unknown (default) < 3 runs
            * intermittent (not enough failures) >3 runs < 0.4 failure rate
            * disable_recommended (enough repeated failures) >3 runs >= 0.4
            * disable_manifest (disable DEFAULT if no other failures)
            * secondary (not first failure in group)
            * success
        """

        failures = {}
        manifest_paths = {}
        for task in tasks:
            try:
                if len(task.results) == 0:
                    continue  # ignore aborted tasks
                for manifest in task.failure_types:
                    if manifest not in failures:
                        failures[manifest] = {"sum_by_label": {}, "path": {}}
                    if manifest not in manifest_paths:
                        manifest_paths[manifest] = []
                    for path_type in task.failure_types[manifest]:
                        path, _type = path_type
                        if path == manifest:
                            path = "DEFAULT"
                        if path not in failures[manifest]["path"]:
                            failures[manifest]["path"][path] = {}
                        if path not in manifest_paths[manifest]:
                            manifest_paths[manifest].append(path)
                        if task.label not in failures[manifest]["sum_by_label"]:
                            failures[manifest]["sum_by_label"][task.label] = {
                                Classification.UNKNOWN: 0,
                                Classification.SECONDARY: 0,
                                Classification.INTERMITTENT: 0,
                                Classification.DISABLE_RECOMMENDED: 0,
                                Classification.DISABLE_MANIFEST: 0,
                                Classification.SUCCESS: 0,
                            }
                        if task.label not in failures[manifest]["path"][path]:
                            failures[manifest]["path"][path][task.label] = {
                                "total_runs": 0,
                                "failed_runs": 0,
                                "classification": Classification.UNKNOWN,
                                "runs": {task.id: False},
                            }
                        else:
                            failures[manifest]["path"][path][task.label]["runs"][
                                task.id
                            ] = False
            except AttributeError as ae:
                self.warning(f"unknown attribute in task: {ae}")

        # calculate success/failure for each known path
        for manifest in manifest_paths:
            manifest_paths[manifest] = sort_paths(manifest_paths[manifest])
        for task in tasks:
            try:
                if len(task.results) == 0:
                    continue  # ignore aborted tasks
                for result in task.results:
                    manifest = result.group
                    if manifest not in failures:
                        self.warning(
                            f"result for {manifest} not in any failures, ignored"
                        )
                        continue
                    for path in manifest_paths[manifest]:
                        if task.label not in failures[manifest]["sum_by_label"]:
                            failures[manifest]["sum_by_label"][task.label] = {
                                Classification.UNKNOWN: 0,
                                Classification.SECONDARY: 0,
                                Classification.INTERMITTENT: 0,
                                Classification.DISABLE_RECOMMENDED: 0,
                                Classification.DISABLE_MANIFEST: 0,
                                Classification.SUCCESS: 0,
                            }
                        if task.label not in failures[manifest]["path"][path]:
                            failures[manifest]["path"][path][task.label] = {
                                "total_runs": 0,
                                "failed_runs": 0,
                                "classification": Classification.UNKNOWN,
                                "runs": {},
                            }
                        if (
                            task.id
                            not in failures[manifest]["path"][path][task.label]["runs"]
                        ):
                            ok = True
                            failures[manifest]["path"][path][task.label]["runs"][
                                task.id
                            ] = ok
                        else:
                            ok = (
                                result.ok
                                or failures[manifest]["path"][path][task.label]["runs"][
                                    task.id
                                ]
                            )
                        failures[manifest]["path"][path][task.label]["total_runs"] += 1
                        if not ok:
                            failures[manifest]["path"][path][task.label][
                                "failed_runs"
                            ] += 1
            except AttributeError as ae:
                self.warning(f"unknown attribute in task: {ae}")

        # classify failures and roll up summary statistics
        for manifest in failures:
            for path in failures[manifest]["path"]:
                for label in failures[manifest]["path"][path]:
                    failed_runs = failures[manifest]["path"][path][label]["failed_runs"]
                    total_runs = failures[manifest]["path"][path][label]["total_runs"]
                    classification = failures[manifest]["path"][path][label][
                        "classification"
                    ]
                    if total_runs >= 3:
                        if failed_runs / total_runs < 0.4:
                            if failed_runs == 0:
                                classification = Classification.SUCCESS
                            else:
                                classification = Classification.INTERMITTENT
                        else:
                            classification = Classification.SECONDARY
                        failures[manifest]["path"][path][label][
                            "classification"
                        ] = classification
                    failures[manifest]["sum_by_label"][label][classification] += 1

        # Identify the first failure (for each test, in a manifest, by label)
        for manifest in failures:
            alpha_paths = sort_paths(failures[manifest]["path"].keys())
            for path in alpha_paths:
                for label in failures[manifest]["path"][path]:
                    primary = (
                        failures[manifest]["sum_by_label"][label][
                            Classification.DISABLE_RECOMMENDED
                        ]
                        == 0
                    )
                    if path == "DEFAULT":
                        classification = failures[manifest]["path"][path][label][
                            "classification"
                        ]
                        if (
                            classification == Classification.SECONDARY
                            and failures[manifest]["sum_by_label"][label][
                                classification
                            ]
                            == 1
                        ):
                            # ONLY failure in the manifest for this label => DISABLE
                            failures[manifest]["path"][path][label][
                                "classification"
                            ] = Classification.DISABLE_MANIFEST
                            failures[manifest]["sum_by_label"][label][
                                classification
                            ] -= 1
                            failures[manifest]["sum_by_label"][label][
                                Classification.DISABLE_MANIFEST
                            ] += 1

                    else:
                        if (
                            primary
                            and failures[manifest]["path"][path][label][
                                "classification"
                            ]
                            == Classification.SECONDARY
                        ):
                            # FIRST failure in the manifest for this label => DISABLE
                            failures[manifest]["path"][path][label][
                                "classification"
                            ] = Classification.DISABLE_RECOMMENDED
                            failures[manifest]["sum_by_label"][label][
                                Classification.SECONDARY
                            ] -= 1
                            failures[manifest]["sum_by_label"][label][
                                Classification.DISABLE_RECOMMENDED
                            ] += 1

        return failures

    def _get_os_version(self, os, platform):
        """Return the os_version given the label platform string"""
        i = platform.find(os)
        j = i + len(os)
        yy = platform[j : j + 2]
        mm = platform[j + 2 : j + 4]
        return yy + "." + mm

    def get_bug_by_id(self, id):
        """Get bug by bug id"""

        self._initialize_bzapi()
        bug = self._bzapi.getbug(id)
        return bug

    def get_bugs_by_summary(self, summary):
        """Get bug by bug summary"""

        self._initialize_bzapi()
        query = self._bzapi.build_query(short_desc=summary)
        query["include_fields"] = [
            "id",
            "product",
            "component",
            "status",
            "resolution",
            "summary",
            "blocks",
        ]
        bugs = self._bzapi.query(query)
        return bugs

    def create_bug(
        self,
        summary="Bug short description",
        description="Bug description",
        product="Testing",
        component="General",
        version="unspecified",
        bugtype="task",
    ):
        """Create a bug"""

        self._initialize_bzapi()
        if not self._bzapi.logged_in:
            self.error(
                "Must create a Bugzilla API key per https://github.com/mozilla/mozci-tools/blob/main/citools/test_triage_bug_filer.py"
            )
            raise PermissionError(f"Not authenticated for Bugzilla {self.bugzilla}")
        createinfo = self._bzapi.build_createbug(
            product=product,
            component=component,
            summary=summary,
            version=version,
            description=description,
        )
        createinfo["type"] = bugtype
        bug = self._bzapi.createbug(createinfo)
        return bug

    def add_bug_comment(self, id, comment, meta_bug_id=None):
        """Add a comment to an existing bug"""

        self._initialize_bzapi()
        if not self._bzapi.logged_in:
            self.error(BUGZILLA_AUTHENTICATION_HELP)
            raise PermissionError("Not authenticated for Bugzilla")
        if meta_bug_id is not None:
            blocks_add = [meta_bug_id]
        else:
            blocks_add = None
        updateinfo = self._bzapi.build_update(comment=comment, blocks_add=blocks_add)
        self._bzapi.update_bugs([id], updateinfo)

    def skip_failure(
        self,
        manifest,
        path,
        label,
        classification,
        task_id,
        try_url,
        revision,
        repo,
        meta_bug_id=None,
    ):
        """Skip a failure"""

        self.vinfo(f"===== Skip failure in manifest: {manifest} =====")
        skip_if = self.task_to_skip_if(task_id)
        if skip_if is None:
            self.warning(
                f"Unable to calculate skip-if condition from manifest={manifest} from failure label={label}"
            )
            return
        bug_reference = ""
        if classification == Classification.DISABLE_MANIFEST:
            filename = "DEFAULT"
            comment = "Disabled entire manifest due to crash result"
        else:
            filename = self.get_filename_in_manifest(manifest, path)
            comment = f'Disabled test due to failures: "{filename}"'
            if classification == Classification.SECONDARY:
                comment += " (secondary)"
                bug_reference = " (secondary)"
        comment += f"\nTry URL = {try_url}"
        comment += f"\nrevision = {revision}"
        comment += f"\nrepo = {repo}"
        comment += f"\nlabel = {label}"
        comment += f"\ntask_id = {task_id}"
        push_id = self.get_push_id(revision, repo)
        if push_id is not None:
            comment += f"\npush_id = {push_id}"
            job_id = self.get_job_id(push_id, task_id)
            if job_id is not None:
                comment += f"\njob_id = {job_id}"
                suggestions_url, line_number, line, log_url = self.get_bug_suggestions(
                    repo, job_id, path
                )
                if log_url is not None:
                    comment += f"\n\nBug suggestions: {suggestions_url}"
                    comment += f"\nSpecifically see at line {line_number} in the attached log: {log_url}"
                    comment += f'\n\n  "{line}"\n'
        platform, testname = self.label_to_platform_testname(label)
        if platform is not None:
            comment += "\n\nCommand line to reproduce:\n\n"
            comment += f"  \"mach try fuzzy -q '{platform}' {testname}\""
        bug_summary = f"MANIFEST {manifest}"
        attachments = {}
        bugs = self.get_bugs_by_summary(bug_summary)
        if len(bugs) == 0:
            description = (
                f"This bug covers excluded failing tests in the MANIFEST {manifest}"
            )
            description += "\n(generated by mach manifest skip-fails)"
            product, component = self.get_file_info(path)
            if self.dry_run:
                self.warning(
                    f'Dry-run NOT creating bug: {product}::{component} "{bug_summary}"'
                )
                bugid = "TBD"
            else:
                bug = self.create_bug(bug_summary, description, product, component)
                bugid = bug.id
                self.vinfo(
                    f'Created Bug {bugid} {product}::{component} : "{bug_summary}"'
                )
            bug_reference = f"Bug {bugid}" + bug_reference
        elif len(bugs) == 1:
            bugid = bugs[0].id
            bug_reference = f"Bug {bugid}" + bug_reference
            product = bugs[0].product
            component = bugs[0].component
            self.vinfo(f'Found Bug {bugid} {product}::{component} "{bug_summary}"')
            if meta_bug_id is not None:
                if meta_bug_id in bugs[0].blocks:
                    self.vinfo(f"  Bug {bugid} already blocks meta bug {meta_bug_id}")
                    meta_bug_id = None  # no need to add again
            comments = bugs[0].getcomments()
            for i in range(len(comments)):
                text = comments[i]["text"]
                m = self._attach_rx.findall(text)
                if len(m) == 1:
                    a_task_id = m[0][1]
                    attachments[a_task_id] = m[0][0]
                    if a_task_id == task_id:
                        self.vinfo(
                            f"  Bug {bugid} already has the compressed log attached for this task"
                        )
        else:
            self.error(f'More than one bug found for summary: "{bug_summary}"')
            return
        if self.dry_run:
            self.warning(f"Dry-run NOT adding comment to Bug {bugid}: {comment}")
            self.info(f'Dry-run NOT editing ["{filename}"] manifest: "{manifest}"')
            self.info(f'would add skip-if condition: "{skip_if}" # {bug_reference}')
            if task_id not in attachments:
                self.info("would add compressed log for this task")
            return
        self.add_bug_comment(bugid, comment, meta_bug_id)
        self.info(f"Added comment to Bug {bugid}: {comment}")
        if meta_bug_id is not None:
            self.info(f"  Bug {bugid} blocks meta Bug: {meta_bug_id}")
        if task_id not in attachments:
            self.add_attachment_log_for_task(bugid, task_id)
            self.info("Added compressed log for this task")
        mp = ManifestParser(use_toml=True, document=True)
        manifest_path = os.path.join(self.topsrcdir, os.path.normpath(manifest))
        mp.read(manifest_path)
        document = mp.source_documents[manifest_path]
        add_skip_if(document, filename, skip_if, bug_reference)
        manifest_str = alphabetize_toml_str(document)
        fp = io.open(manifest_path, "w", encoding="utf-8", newline="\n")
        fp.write(manifest_str)
        fp.close()
        self.info(f'Edited ["{filename}"] in manifest: "{manifest}"')
        self.info(f'added skip-if condition: "{skip_if}" # {bug_reference}')

    def get_variants(self):
        """Get mozinfo for each test variants"""

        if len(self.variants) == 0:
            variants_file = "taskcluster/ci/test/variants.yml"
            variants_path = os.path.join(
                self.topsrcdir, os.path.normpath(variants_file)
            )
            fp = io.open(variants_path, "r", encoding="utf-8")
            raw_variants = load(fp, Loader=Loader)
            fp.close()
            for k, v in raw_variants.items():
                mozinfo = k
                if "mozinfo" in v:
                    mozinfo = v["mozinfo"]
                self.variants[k] = mozinfo
        return self.variants

    def get_task(self, task_id):
        """Download details for task task_id"""

        if task_id in self.tasks:  # if cached
            task = self.tasks[task_id]
        else:
            task = get_task(task_id)
            self.tasks[task_id] = task
        return task

    def task_to_skip_if(self, task_id):
        """Calculate the skip-if condition for failing task task_id"""

        self.get_variants()
        task = self.get_task(task_id)
        os = None
        os_version = None
        bits = None
        display = None
        runtimes = []
        build_types = []
        test_setting = task.get("extra", {}).get("test-setting", {})
        platform = test_setting.get("platform", {})
        platform_os = platform.get("os", {})
        if "name" in platform_os:
            os = platform_os["name"]
            if os == "windows":
                os = "win"
            if os == "macosx":
                os = "mac"
        if "version" in platform_os:
            os_version = platform_os["version"]
            if len(os_version) == 4:
                os_version = os_version[0:2] + "." + os_version[2:4]
        if "arch" in platform:
            arch = platform["arch"]
            if arch == "x86" or arch.find("32") >= 0:
                bits = "32"
        if "display" in platform:
            display = platform["display"]
        if "runtime" in test_setting:
            for k in test_setting["runtime"]:
                if k in self.variants:
                    runtimes.append(self.variants[k])  # adds mozinfo
        if "build" in test_setting:
            tbuild = test_setting["build"]
            opt = False
            debug = False
            for k in tbuild:
                if k == "type":
                    if tbuild[k] == "opt":
                        opt = True
                    elif tbuild[k] == "debug":
                        debug = True
                else:
                    build_types.append(k)
            if len(build_types) == 0:
                if opt:
                    build_types.append("!debug")
                if debug:
                    build_types.append("debug")
        skip_if = None
        if os is not None:
            skip_if = "os == '" + os + "'"
            if os_version is not None:
                skip_if += " && "
                skip_if += "os_version == '" + os_version + "'"
            if bits is not None:
                skip_if += " && "
                skip_if += "bits == '" + bits + "'"
            if display is not None:
                skip_if += " && "
                skip_if += "display == '" + display + "'"
            for runtime in runtimes:
                skip_if += " && "
                skip_if += runtime
            for build_type in build_types:
                skip_if += " && "
                skip_if += build_type
        return skip_if

    def get_file_info(self, path, product="Testing", component="General"):
        """
        Get bugzilla product and component for the path.
        Provide defaults (in case command_context is not defined
        or there isn't file info available).
        """
        if path != "DEFAULT" and self.command_context is not None:
            reader = self.command_context.mozbuild_reader(config_mode="empty")
            info = reader.files_info([path])
            cp = info[path]["BUG_COMPONENT"]
            product = cp.product
            component = cp.component
        return product, component

    def get_filename_in_manifest(self, manifest, path):
        """return relative filename for path in manifest"""

        filename = os.path.basename(path)
        if filename == "DEFAULT":
            return filename
        manifest_dir = os.path.dirname(manifest)
        i = 0
        j = min(len(manifest_dir), len(path))
        while i < j and manifest_dir[i] == path[i]:
            i += 1
        if i < len(manifest_dir):
            for _ in range(manifest_dir.count("/", i) + 1):
                filename = "../" + filename
        elif i < len(path):
            filename = path[i + 1 :]
        return filename

    def get_push_id(self, revision, repo):
        """Return the push_id for revision and repo (or None)"""

        self.vinfo(f"Retrieving push_id for {repo} revision: {revision} ...")
        if revision in self.push_ids:  # if cached
            push_id = self.push_ids[revision]
        else:
            push_id = None
            push_url = f"https://treeherder.mozilla.org/api/project/{repo}/push/"
            params = {}
            params["full"] = "true"
            params["count"] = 10
            params["revision"] = revision
            r = requests.get(push_url, headers=self.headers, params=params)
            if r.status_code != 200:
                self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
            else:
                response = r.json()
                if "results" in response:
                    results = response["results"]
                    if len(results) > 0:
                        r0 = results[0]
                        if "id" in r0:
                            push_id = r0["id"]
            self.push_ids[revision] = push_id
        return push_id

    def get_job_id(self, push_id, task_id):
        """Return the job_id for push_id, task_id (or None)"""

        self.vinfo(f"Retrieving job_id for push_id: {push_id}, task_id: {task_id} ...")
        if push_id in self.job_ids:  # if cached
            job_id = self.job_ids[push_id]
        else:
            job_id = None
            params = {}
            params["push_id"] = push_id
            r = requests.get(self.jobs_url, headers=self.headers, params=params)
            if r.status_code != 200:
                self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
            else:
                response = r.json()
                if "results" in response:
                    results = response["results"]
                    if len(results) > 0:
                        for result in results:
                            if len(result) > 14:
                                if result[14] == task_id:
                                    job_id = result[1]
                                    break
            self.job_ids[push_id] = job_id
        return job_id

    def get_bug_suggestions(self, repo, job_id, path):
        """
        Return the (suggestions_url, line_number, line, log_url)
        for the given repo and job_id
        """
        self.vinfo(
            f"Retrieving bug_suggestions for {repo} job_id: {job_id}, path: {path} ..."
        )
        suggestions_url = f"https://treeherder.mozilla.org/api/project/{repo}/jobs/{job_id}/bug_suggestions/"
        line_number = None
        line = None
        log_url = None
        r = requests.get(suggestions_url, headers=self.headers)
        if r.status_code != 200:
            self.warning(f"FAILED to query Treeherder = {r} for {r.url}")
        else:
            response = r.json()
            if len(response) > 0:
                for sugg in response:
                    if sugg["path_end"] == path:
                        line_number = sugg["line_number"] + 1
                        line = sugg["search"]
                        log_url = f"https://treeherder.mozilla.org/logviewer?repo={repo}&job_id={job_id}&lineNumber={line_number}"
                        break
        rv = (suggestions_url, line_number, line, log_url)
        return rv

    def read_json(self, filename):
        """read data as JSON from filename"""
        fp = io.open(filename, "r", encoding="utf-8")
        data = json.load(fp)
        fp.close()
        return data

    def write_json(self, filename, data):
        """saves data as JSON to filename"""
        fp = io.open(filename, "w", encoding="utf-8")
        json.dump(data, fp, indent=2, sort_keys=True)
        fp.close()

    def write_tasks(self, save_tasks, tasks):
        """saves tasks as JSON to save_tasks"""
        jtasks = []
        for task in tasks:
            if not isinstance(task, TestTask):
                continue
            jtask = {}
            jtask["id"] = task.id
            jtask["label"] = task.label
            jtask["duration"] = task.duration
            jtask["result"] = task.result
            jtask["state"] = task.state
            jtags = {}
            for k, v in task.tags.items():
                if k == "createdForUser":
                    jtags[k] = "ci@mozilla.com"
                else:
                    jtags[k] = v
            jtask["tags"] = jtags
            jtask["tier"] = task.tier
            jtask["results"] = [
                {"group": r.group, "ok": r.ok, "duration": r.duration}
                for r in task.results
            ]
            jtask["errors"] = None  # Bug with task.errors property??
            jft = {}
            for k in task.failure_types:
                jft[k] = [[f[0], f[1].value] for f in task.failure_types[k]]
            jtask["failure_types"] = jft
            jtasks.append(jtask)
        self.write_json(save_tasks, jtasks)

    def label_to_platform_testname(self, label):
        """convert from label to platform, testname for mach command line"""
        platform = None
        testname = None
        platform_details = label.split("/")
        if len(platform_details) == 2:
            platform, details = platform_details
            words = details.split("-")
            if len(words) > 2:
                platform += "/" + words.pop(0)  # opt or debug
                try:
                    _chunk = int(words[-1])
                    words.pop()
                except ValueError:
                    pass
                words.pop()  # remove test suffix
                testname = "-".join(words)
            else:
                platform = None
        return platform, testname

    def add_attachment_log_for_task(self, bugid, task_id):
        """Adds compressed log for this task to bugid"""

        log_url = f"https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/{task_id}/artifacts/public/logs/live_backing.log"
        r = requests.get(log_url, headers=self.headers)
        if r.status_code != 200:
            self.error(f"Unable get log for task: {task_id}")
            return
        attach_fp = tempfile.NamedTemporaryFile()
        fp = gzip.open(attach_fp, "wb")
        fp.write(r.text.encode("utf-8"))
        fp.close()
        self._initialize_bzapi()
        description = ATTACHMENT_DESCRIPTION + task_id
        file_name = TASK_LOG + ".gz"
        comment = "Added compressed log"
        content_type = "application/gzip"
        try:
            self._bzapi.attachfile(
                [bugid],
                attach_fp.name,
                description,
                file_name=file_name,
                comment=comment,
                content_type=content_type,
                is_private=False,
            )
        except Fault:
            pass  # Fault expected: Failed to fetch key 9372091 from network storage: The specified key does not exist.
