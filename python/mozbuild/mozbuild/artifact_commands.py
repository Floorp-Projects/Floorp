# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import argparse
import hashlib
import json
import logging
import os
import shutil
from collections import OrderedDict

# As a result of the selective module loading changes, this import has to be
# done here. It is not explicitly used, but it has an implicit side-effect
# (bringing in TASKCLUSTER_ROOT_URL) which is necessary.
import gecko_taskgraph.main  # noqa: F401
import mozversioncontrol
import six
from mach.decorators import Command, CommandArgument, SubCommand

from mozbuild.artifact_builds import JOB_CHOICES
from mozbuild.base import MachCommandConditions as conditions
from mozbuild.dirutils import ensureParentDir

_COULD_NOT_FIND_ARTIFACTS_TEMPLATE = (
    "ERROR!!!!!! Could not find artifacts for a toolchain build named "
    "`{build}`. Local commits, dirty/stale files, and other changes in your "
    "checkout may cause this error. Make sure you are on a fresh, current "
    "checkout of mozilla-central. Beware that commands like `mach bootstrap` "
    "and `mach artifact` are unlikely to work on any versions of the code "
    "besides recent revisions of mozilla-central."
)


class SymbolsAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        # If this function is called, it means the --symbols option was given,
        # so we want to store the value `True` if no explicit value was given
        # to the option.
        setattr(namespace, self.dest, values or True)


class ArtifactSubCommand(SubCommand):
    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        args = [
            CommandArgument("--tree", metavar="TREE", type=str, help="Firefox tree."),
            CommandArgument(
                "--job", metavar="JOB", choices=JOB_CHOICES, help="Build job."
            ),
            CommandArgument(
                "--verbose", "-v", action="store_true", help="Print verbose output."
            ),
        ]
        for arg in args:
            after = arg(after)
        return after


# Fetch and install binary artifacts from Mozilla automation.


@Command(
    "artifact",
    category="post-build",
    description="Use pre-built artifacts to build Firefox.",
)
def artifact(command_context):
    """Download, cache, and install pre-built binary artifacts to build Firefox.

    Use ``mach build`` as normal to freshen your installed binary libraries:
    artifact builds automatically download, cache, and install binary
    artifacts from Mozilla automation, replacing whatever may be in your
    object directory.  Use ``mach artifact last`` to see what binary artifacts
    were last used.

    Never build libxul again!

    """
    pass


def _make_artifacts(
    command_context,
    tree=None,
    job=None,
    skip_cache=False,
    download_tests=True,
    download_symbols=False,
    download_maven_zip=False,
    no_process=False,
):
    state_dir = command_context._mach_context.state_dir
    cache_dir = os.path.join(state_dir, "package-frontend")

    hg = None
    if conditions.is_hg(command_context):
        hg = command_context.substs["HG"]

    git = None
    if conditions.is_git(command_context):
        git = command_context.substs["GIT"]

    # If we're building Thunderbird, we should be checking for comm-central artifacts.
    topsrcdir = command_context.substs.get("commtopsrcdir", command_context.topsrcdir)

    if download_maven_zip:
        if download_tests:
            raise ValueError("--maven-zip requires --no-tests")
        if download_symbols:
            raise ValueError("--maven-zip requires no --symbols")
        if not no_process:
            raise ValueError("--maven-zip requires --no-process")

    from mozbuild.artifacts import Artifacts

    artifacts = Artifacts(
        tree,
        command_context.substs,
        command_context.defines,
        job,
        log=command_context.log,
        cache_dir=cache_dir,
        skip_cache=skip_cache,
        hg=hg,
        git=git,
        topsrcdir=topsrcdir,
        download_tests=download_tests,
        download_symbols=download_symbols,
        download_maven_zip=download_maven_zip,
        no_process=no_process,
        mozbuild=command_context,
    )
    return artifacts


@ArtifactSubCommand(
    "artifact",
    "install",
    "Install a good pre-built artifact.",
)
@CommandArgument(
    "source",
    metavar="SRC",
    nargs="?",
    type=str,
    help="Where to fetch and install artifacts from.  Can be omitted, in "
    "which case the current hg repository is inspected; an hg revision; "
    "a remote URL; or a local file.",
    default=None,
)
@CommandArgument(
    "--skip-cache",
    action="store_true",
    help="Skip all local caches to force re-fetching remote artifacts.",
    default=False,
)
@CommandArgument("--no-tests", action="store_true", help="Don't install tests.")
@CommandArgument("--symbols", nargs="?", action=SymbolsAction, help="Download symbols.")
@CommandArgument("--distdir", help="Where to install artifacts to.")
@CommandArgument(
    "--no-process",
    action="store_true",
    help="Don't process (unpack) artifact packages, just download them.",
)
@CommandArgument(
    "--maven-zip", action="store_true", help="Download Maven zip (Android-only)."
)
def artifact_install(
    command_context,
    source=None,
    skip_cache=False,
    tree=None,
    job=None,
    verbose=False,
    no_tests=False,
    symbols=False,
    distdir=None,
    no_process=False,
    maven_zip=False,
):
    command_context._set_log_level(verbose)
    artifacts = _make_artifacts(
        command_context,
        tree=tree,
        job=job,
        skip_cache=skip_cache,
        download_tests=not no_tests,
        download_symbols=symbols,
        download_maven_zip=maven_zip,
        no_process=no_process,
    )

    return artifacts.install_from(source, distdir or command_context.distdir)


@ArtifactSubCommand(
    "artifact",
    "clear-cache",
    "Delete local artifacts and reset local artifact cache.",
)
def artifact_clear_cache(command_context, tree=None, job=None, verbose=False):
    command_context._set_log_level(verbose)
    artifacts = _make_artifacts(command_context, tree=tree, job=job)
    artifacts.clear_cache()
    return 0


@SubCommand(
    "artifact",
    "toolchain",
)
@CommandArgument("--verbose", "-v", action="store_true", help="Print verbose output.")
@CommandArgument(
    "--cache-dir",
    metavar="DIR",
    help="Directory where to store the artifacts cache",
)
@CommandArgument(
    "--skip-cache",
    action="store_true",
    help="Skip all local caches to force re-fetching remote artifacts.",
    default=False,
)
@CommandArgument(
    "--from-build",
    metavar="BUILD",
    nargs="+",
    help="Download toolchains resulting from the given build(s); "
    "BUILD is a name of a toolchain task, e.g. linux64-clang",
)
@CommandArgument(
    "--from-task",
    metavar="TASK_ID:ARTIFACT",
    nargs="+",
    help="Download toolchain artifact from a given task.",
)
@CommandArgument(
    "--tooltool-manifest",
    metavar="MANIFEST",
    help="Explicit tooltool manifest to process",
)
@CommandArgument(
    "--no-unpack", action="store_true", help="Do not unpack any downloaded file"
)
@CommandArgument(
    "--retry", type=int, default=4, help="Number of times to retry failed downloads"
)
@CommandArgument(
    "--bootstrap",
    action="store_true",
    help="Whether this is being called from bootstrap. "
    "This verifies the toolchain is annotated as a toolchain used for local development.",
)
@CommandArgument(
    "--artifact-manifest",
    metavar="FILE",
    help="Store a manifest about the downloaded taskcluster artifacts",
)
def artifact_toolchain(
    command_context,
    verbose=False,
    cache_dir=None,
    skip_cache=False,
    from_build=(),
    from_task=(),
    tooltool_manifest=None,
    no_unpack=False,
    retry=0,
    bootstrap=False,
    artifact_manifest=None,
):
    """Download, cache and install pre-built toolchains."""
    import time

    import redo
    import requests
    from taskgraph.util.taskcluster import get_artifact_url

    from mozbuild.action.tooltool import FileRecord, open_manifest, unpack_file
    from mozbuild.artifacts import ArtifactCache

    start = time.monotonic()
    command_context._set_log_level(verbose)
    # Normally, we'd use command_context.log_manager.enable_unstructured(),
    # but that enables all logging, while we only really want tooltool's
    # and it also makes structured log output twice.
    # So we manually do what it does, and limit that to the tooltool
    # logger.
    if command_context.log_manager.terminal_handler:
        logging.getLogger("mozbuild.action.tooltool").addHandler(
            command_context.log_manager.terminal_handler
        )
        logging.getLogger("redo").addHandler(
            command_context.log_manager.terminal_handler
        )
        command_context.log_manager.terminal_handler.addFilter(
            command_context.log_manager.structured_filter
        )
    if not cache_dir:
        cache_dir = os.path.join(command_context._mach_context.state_dir, "toolchains")

    tooltool_host = os.environ.get("TOOLTOOL_HOST", "tooltool.mozilla-releng.net")
    taskcluster_proxy_url = os.environ.get("TASKCLUSTER_PROXY_URL")
    if taskcluster_proxy_url:
        tooltool_url = "{}/{}".format(taskcluster_proxy_url, tooltool_host)
    else:
        tooltool_url = "https://{}".format(tooltool_host)

    cache = ArtifactCache(
        cache_dir=cache_dir, log=command_context.log, skip_cache=skip_cache
    )

    class DownloadRecord(FileRecord):
        def __init__(self, url, *args, **kwargs):
            super(DownloadRecord, self).__init__(*args, **kwargs)
            self.url = url
            self.basename = self.filename

        def fetch_with(self, cache):
            self.filename = cache.fetch(self.url)
            return self.filename

        def validate(self):
            if self.size is None and self.digest is None:
                return True
            return super(DownloadRecord, self).validate()

    class ArtifactRecord(DownloadRecord):
        def __init__(self, task_id, artifact_name):
            for _ in redo.retrier(attempts=retry + 1, sleeptime=60):
                cot = cache._download_manager.session.get(
                    get_artifact_url(task_id, "public/chain-of-trust.json")
                )
                if cot.status_code >= 500:
                    continue
                cot.raise_for_status()
                break
            else:
                cot.raise_for_status()

            digest = algorithm = None
            data = json.loads(cot.text)
            for algorithm, digest in (
                data.get("artifacts", {}).get(artifact_name, {}).items()
            ):
                pass

            name = os.path.basename(artifact_name)
            artifact_url = get_artifact_url(
                task_id,
                artifact_name,
                use_proxy=not artifact_name.startswith("public/"),
            )
            super(ArtifactRecord, self).__init__(
                artifact_url, name, None, digest, algorithm, unpack=True
            )

    records = OrderedDict()
    downloaded = []

    if tooltool_manifest:
        manifest = open_manifest(tooltool_manifest)
        for record in manifest.file_records:
            url = "{}/{}/{}".format(tooltool_url, record.algorithm, record.digest)
            records[record.filename] = DownloadRecord(
                url,
                record.filename,
                record.size,
                record.digest,
                record.algorithm,
                unpack=record.unpack,
                version=record.version,
                visibility=record.visibility,
            )

    if from_build:
        if "MOZ_AUTOMATION" in os.environ:
            command_context.log(
                logging.ERROR,
                "artifact",
                {},
                "Do not use --from-build in automation; all dependencies "
                "should be determined in the decision task.",
            )
            return 1
        from taskgraph.optimize.strategies import IndexSearch

        from mozbuild.toolchains import toolchain_task_definitions

        tasks = toolchain_task_definitions()

        for b in from_build:
            user_value = b

            if not b.startswith("toolchain-"):
                b = "toolchain-{}".format(b)

            task = tasks.get(b)
            if not task:
                command_context.log(
                    logging.ERROR,
                    "artifact",
                    {"build": user_value},
                    "Could not find a toolchain build named `{build}`",
                )
                return 1

            # Ensure that toolchains installed by `mach bootstrap` have the
            # `local-toolchain attribute set. Taskgraph ensures that these
            # are built on trunk projects, so the task will be available to
            # install here.
            if bootstrap and not task.attributes.get("local-toolchain"):
                command_context.log(
                    logging.ERROR,
                    "artifact",
                    {"build": user_value},
                    "Toolchain `{build}` is not annotated as used for local development.",
                )
                return 1

            artifact_name = task.attributes.get("toolchain-artifact")
            command_context.log(
                logging.DEBUG,
                "artifact",
                {
                    "name": artifact_name,
                    "index": task.optimization.get("index-search"),
                },
                "Searching for {name} in {index}",
            )
            deadline = None
            task_id = IndexSearch().should_replace_task(
                task, {}, deadline, task.optimization.get("index-search", [])
            )
            if task_id in (True, False) or not artifact_name:
                command_context.log(
                    logging.ERROR,
                    "artifact",
                    {"build": user_value},
                    _COULD_NOT_FIND_ARTIFACTS_TEMPLATE,
                )
                # Get and print some helpful info for diagnosis.
                repo = mozversioncontrol.get_repository_object(
                    command_context.topsrcdir
                )
                if not isinstance(repo, mozversioncontrol.SrcRepository):
                    changed_files = set(repo.get_outgoing_files()) | set(
                        repo.get_changed_files()
                    )
                    if changed_files:
                        command_context.log(
                            logging.ERROR,
                            "artifact",
                            {},
                            "Hint: consider reverting your local changes "
                            "to the following files: %s" % sorted(changed_files),
                        )
                if "TASKCLUSTER_ROOT_URL" in os.environ:
                    command_context.log(
                        logging.ERROR,
                        "artifact",
                        {"build": user_value},
                        "Due to the environment variable TASKCLUSTER_ROOT_URL "
                        "being set, the artifacts were expected to be found "
                        "on {}. If this was unintended, unset "
                        "TASKCLUSTER_ROOT_URL and try again.".format(
                            os.environ["TASKCLUSTER_ROOT_URL"]
                        ),
                    )
                return 1

            command_context.log(
                logging.DEBUG,
                "artifact",
                {"name": artifact_name, "task_id": task_id},
                "Found {name} in {task_id}",
            )

            record = ArtifactRecord(task_id, artifact_name)
            records[record.filename] = record

    # Handle the list of files of the form task_id:path from --from-task.
    for f in from_task or ():
        task_id, colon, name = f.partition(":")
        if not colon:
            command_context.log(
                logging.ERROR,
                "artifact",
                {},
                "Expected an argument of the form task_id:path",
            )
            return 1
        record = ArtifactRecord(task_id, name)
        records[record.filename] = record

    for record in six.itervalues(records):
        command_context.log(
            logging.INFO,
            "artifact",
            {"name": record.basename},
            "Setting up artifact {name}",
        )
        valid = False
        # sleeptime is 60 per retry.py, used by tooltool_wrapper.sh
        for attempt, _ in enumerate(redo.retrier(attempts=retry + 1, sleeptime=60)):
            try:
                record.fetch_with(cache)
            except (
                requests.exceptions.HTTPError,
                requests.exceptions.ChunkedEncodingError,
                requests.exceptions.ConnectionError,
            ) as e:
                if isinstance(e, requests.exceptions.HTTPError):
                    # The relengapi proxy likes to return error 400 bad request
                    # which seems improbably to be due to our (simple) GET
                    # being borked.
                    status = e.response.status_code
                    should_retry = status >= 500 or status == 400
                else:
                    should_retry = True

                if should_retry or attempt < retry:
                    level = logging.WARN
                else:
                    level = logging.ERROR
                command_context.log(level, "artifact", {}, str(e))
                if not should_retry:
                    break
                if attempt < retry:
                    command_context.log(
                        logging.INFO, "artifact", {}, "Will retry in a moment..."
                    )
                continue
            try:
                valid = record.validate()
            except Exception:
                pass
            if not valid:
                os.unlink(record.filename)
                if attempt < retry:
                    command_context.log(
                        logging.INFO,
                        "artifact",
                        {},
                        "Corrupt download. Will retry in a moment...",
                    )
                continue

            downloaded.append(record)
            break

        if not valid:
            command_context.log(
                logging.ERROR,
                "artifact",
                {"name": record.basename},
                "Failed to download {name}",
            )
            return 1

    artifacts = {} if artifact_manifest else None

    for record in downloaded:
        local = os.path.join(os.getcwd(), record.basename)
        if os.path.exists(local):
            os.unlink(local)
        # unpack_file needs the file with its final name to work
        # (https://github.com/mozilla/build-tooltool/issues/38), so we
        # need to copy it, even though we remove it later. Use hard links
        # when possible.
        try:
            os.link(record.filename, local)
        except Exception:
            shutil.copy(record.filename, local)
        # Keep a sha256 of each downloaded file, for the chain-of-trust
        # validation.
        if artifact_manifest is not None:
            with open(local, "rb") as fh:
                h = hashlib.sha256()
                while True:
                    data = fh.read(1024 * 1024)
                    if not data:
                        break
                    h.update(data)
            artifacts[record.url] = {"sha256": h.hexdigest()}
        if record.unpack and not no_unpack:
            unpack_file(local)
            os.unlink(local)

    if not downloaded:
        command_context.log(logging.ERROR, "artifact", {}, "Nothing to download")
        if from_task:
            return 1

    if artifacts:
        ensureParentDir(artifact_manifest)
        with open(artifact_manifest, "w") as fh:
            json.dump(artifacts, fh, indent=4, sort_keys=True)

    if "MOZ_AUTOMATION" in os.environ:
        end = time.monotonic()

        perfherder_data = {
            "framework": {"name": "build_metrics"},
            "suites": [
                {
                    "name": "mach_artifact_toolchain",
                    "value": end - start,
                    "lowerIsBetter": True,
                    "shouldAlert": False,
                    "subtests": [],
                }
            ],
        }
        command_context.log(
            logging.INFO,
            "perfherder",
            {"data": json.dumps(perfherder_data)},
            "PERFHERDER_DATA: {data}",
        )

    return 0
