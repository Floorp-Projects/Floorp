from __future__ import absolute_import
import argparse
import hashlib
import json
import logging
import os
import shutil

from collections import OrderedDict

import mozpack.path as mozpath

from mozbuild.artifact_builds import JOB_CHOICES

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import (
    MachCommandBase,
    MachCommandConditions as conditions,
)

from mozbuild.util import ensureParentDir


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
            CommandArgument('--tree', metavar='TREE', type=str,
                            help='Firefox tree.'),
            CommandArgument('--job', metavar='JOB', choices=JOB_CHOICES,
                            help='Build job.'),
            CommandArgument('--verbose', '-v', action='store_true',
                            help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


@CommandProvider
class PackageFrontend(MachCommandBase):
    """Fetch and install binary artifacts from Mozilla automation."""

    @Command('artifact', category='post-build',
             description='Use pre-built artifacts to build Firefox.')
    def artifact(self):
        '''Download, cache, and install pre-built binary artifacts to build Firefox.

        Use |mach build| as normal to freshen your installed binary libraries:
        artifact builds automatically download, cache, and install binary
        artifacts from Mozilla automation, replacing whatever may be in your
        object directory.  Use |mach artifact last| to see what binary artifacts
        were last used.

        Never build libxul again!

        '''
        pass

    def _make_artifacts(self, tree=None, job=None, skip_cache=False,
                        download_tests=True, download_symbols=False,
                        download_host_bins=False,
                        download_maven_zip=False,
                        no_process=False):
        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        hg = None
        if conditions.is_hg(self):
            hg = self.substs['HG']

        git = None
        if conditions.is_git(self):
            git = self.substs['GIT']

        # If we're building Thunderbird, we should be checking for comm-central artifacts.
        topsrcdir = self.substs.get('commtopsrcdir', self.topsrcdir)

        if download_maven_zip:
            if download_tests:
                raise ValueError('--maven-zip requires --no-tests')
            if download_symbols:
                raise ValueError('--maven-zip requires no --symbols')
            if download_host_bins:
                raise ValueError('--maven-zip requires no --host-bins')
            if not no_process:
                raise ValueError('--maven-zip requires --no-process')

        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, self.substs, self.defines, job,
                              log=self.log, cache_dir=cache_dir,
                              skip_cache=skip_cache, hg=hg, git=git,
                              topsrcdir=topsrcdir,
                              download_tests=download_tests,
                              download_symbols=download_symbols,
                              download_host_bins=download_host_bins,
                              download_maven_zip=download_maven_zip,
                              no_process=no_process)
        return artifacts

    @ArtifactSubCommand('artifact', 'install',
                        'Install a good pre-built artifact.')
    @CommandArgument('source', metavar='SRC', nargs='?', type=str,
                     help='Where to fetch and install artifacts from.  Can be omitted, in '
                     'which case the current hg repository is inspected; an hg revision; '
                     'a remote URL; or a local file.',
                     default=None)
    @CommandArgument('--skip-cache', action='store_true',
                     help='Skip all local caches to force re-fetching remote artifacts.',
                     default=False)
    @CommandArgument('--no-tests', action='store_true', help="Don't install tests.")
    @CommandArgument('--symbols', nargs='?', action=SymbolsAction, help='Download symbols.')
    @CommandArgument('--host-bins', action='store_true', help='Download host binaries.')
    @CommandArgument('--distdir', help='Where to install artifacts to.')
    @CommandArgument('--no-process', action='store_true',
                     help="Don't process (unpack) artifact packages, just download them.")
    @CommandArgument('--maven-zip', action='store_true', help="Download Maven zip (Android-only).")
    def artifact_install(self, source=None, skip_cache=False, tree=None, job=None, verbose=False,
                         no_tests=False, symbols=False, host_bins=False, distdir=None,
                         no_process=False, maven_zip=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job, skip_cache=skip_cache,
                                         download_tests=not no_tests,
                                         download_symbols=symbols,
                                         download_host_bins=host_bins,
                                         download_maven_zip=maven_zip,
                                         no_process=no_process)

        return artifacts.install_from(source, distdir or self.distdir)

    @ArtifactSubCommand('artifact', 'clear-cache',
                        'Delete local artifacts and reset local artifact cache.')
    def artifact_clear_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0

    @SubCommand('artifact', 'toolchain')
    @CommandArgument('--verbose', '-v', action='store_true',
                     help='Print verbose output.')
    @CommandArgument('--cache-dir', metavar='DIR',
                     help='Directory where to store the artifacts cache')
    @CommandArgument('--skip-cache', action='store_true',
                     help='Skip all local caches to force re-fetching remote artifacts.',
                     default=False)
    @CommandArgument('--from-build', metavar='BUILD', nargs='+',
                     help='Download toolchains resulting from the given build(s); '
                     'BUILD is a name of a toolchain task, e.g. linux64-clang')
    @CommandArgument('--tooltool-manifest', metavar='MANIFEST',
                     help='Explicit tooltool manifest to process')
    @CommandArgument('--authentication-file', metavar='FILE',
                     help='Use the RelengAPI token found in the given file to authenticate')
    @CommandArgument('--tooltool-url', metavar='URL',
                     help='Use the given url as tooltool server')
    @CommandArgument('--no-unpack', action='store_true',
                     help='Do not unpack any downloaded file')
    @CommandArgument('--retry', type=int, default=4,
                     help='Number of times to retry failed downloads')
    @CommandArgument('--artifact-manifest', metavar='FILE',
                     help='Store a manifest about the downloaded taskcluster artifacts')
    @CommandArgument('files', nargs='*',
                     help='A list of files to download, in the form path@task-id, in '
                     'addition to the files listed in the tooltool manifest.')
    def artifact_toolchain(self, verbose=False, cache_dir=None,
                           skip_cache=False, from_build=(),
                           tooltool_manifest=None, authentication_file=None,
                           tooltool_url=None, no_unpack=False, retry=None,
                           artifact_manifest=None, files=()):
        '''Download, cache and install pre-built toolchains.
        '''
        from mozbuild.artifacts import ArtifactCache
        from mozbuild.action.tooltool import (
            FileRecord,
            open_manifest,
            unpack_file,
        )
        from requests.adapters import HTTPAdapter
        import redo
        import requests

        from taskgraph.util.taskcluster import (
            get_artifact_url,
        )

        self._set_log_level(verbose)
        # Normally, we'd use self.log_manager.enable_unstructured(),
        # but that enables all logging, while we only really want tooltool's
        # and it also makes structured log output twice.
        # So we manually do what it does, and limit that to the tooltool
        # logger.
        if self.log_manager.terminal_handler:
            logging.getLogger('mozbuild.action.tooltool').addHandler(
                self.log_manager.terminal_handler)
            logging.getLogger('redo').addHandler(
                self.log_manager.terminal_handler)
            self.log_manager.terminal_handler.addFilter(
                self.log_manager.structured_filter)
        if not cache_dir:
            cache_dir = os.path.join(self._mach_context.state_dir, 'toolchains')

        tooltool_url = (tooltool_url or
                        'https://tooltool.mozilla-releng.net').rstrip('/')

        cache = ArtifactCache(cache_dir=cache_dir, log=self.log,
                              skip_cache=skip_cache)

        if authentication_file:
            with open(authentication_file, 'rb') as f:
                token = f.read().strip()

            class TooltoolAuthenticator(HTTPAdapter):
                def send(self, request, *args, **kwargs):
                    request.headers['Authorization'] = \
                        'Bearer {}'.format(token)
                    return super(TooltoolAuthenticator, self).send(
                        request, *args, **kwargs)

            cache._download_manager.session.mount(
                tooltool_url, TooltoolAuthenticator())

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
                for _ in redo.retrier(attempts=retry+1, sleeptime=60):
                    cot = cache._download_manager.session.get(
                        get_artifact_url(task_id, 'public/chain-of-trust.json'))
                    if cot.status_code >= 500:
                        continue
                    cot.raise_for_status()
                    break
                else:
                    cot.raise_for_status()

                digest = algorithm = None
                data = json.loads(cot.content)
                for algorithm, digest in (data.get('artifacts', {})
                                              .get(artifact_name, {}).items()):
                    pass

                name = os.path.basename(artifact_name)
                artifact_url = get_artifact_url(task_id, artifact_name,
                                                use_proxy=not artifact_name.startswith('public/'))
                super(ArtifactRecord, self).__init__(
                    artifact_url, name,
                    None, digest, algorithm, unpack=True)

        records = OrderedDict()
        downloaded = []

        if tooltool_manifest:
            manifest = open_manifest(tooltool_manifest)
            for record in manifest.file_records:
                url = '{}/{}/{}'.format(tooltool_url, record.algorithm,
                                        record.digest)
                records[record.filename] = DownloadRecord(
                    url, record.filename, record.size, record.digest,
                    record.algorithm, unpack=record.unpack,
                    version=record.version, visibility=record.visibility)

        if from_build:
            if 'MOZ_AUTOMATION' in os.environ:
                self.log(logging.ERROR, 'artifact', {},
                         'Do not use --from-build in automation; all dependencies '
                         'should be determined in the decision task.')
                return 1
            from taskgraph.optimize.strategies import IndexSearch
            from taskgraph.parameters import Parameters
            from taskgraph.generator import load_tasks_for_kind
            params = Parameters(
                level=os.environ.get('MOZ_SCM_LEVEL', '3'),
                strict=False,
            )

            root_dir = mozpath.join(self.topsrcdir, 'taskcluster/ci')
            toolchains = load_tasks_for_kind(params, 'toolchain', root_dir=root_dir)

            aliases = {}
            for t in toolchains.values():
                alias = t.attributes.get('toolchain-alias')
                if alias:
                    aliases['toolchain-{}'.format(alias)] = \
                        t.task['metadata']['name']

            for b in from_build:
                user_value = b

                if not b.startswith('toolchain-'):
                    b = 'toolchain-{}'.format(b)

                task = toolchains.get(aliases.get(b, b))
                if not task:
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find a toolchain build named `{build}`')
                    return 1

                task_id = IndexSearch().should_replace_task(
                    task, {}, task.optimization.get('index-search', []))
                artifact_name = task.attributes.get('toolchain-artifact')
                if task_id in (True, False) or not artifact_name:
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find artifacts for a toolchain build '
                             'named `{build}`. Local commits and other changes '
                             'in your checkout may cause this error. Try '
                             'updating to a fresh checkout of mozilla-central '
                             'to use artifact builds.')
                    return 1

                record = ArtifactRecord(task_id, artifact_name)
                records[record.filename] = record

        # Handle the list of files of the form path@task-id on the command
        # line. Each of those give a path to an artifact to download.
        for f in files:
            if '@' not in f:
                self.log(logging.ERROR, 'artifact', {},
                         'Expected a list of files of the form path@task-id')
                return 1
            name, task_id = f.rsplit('@', 1)
            record = ArtifactRecord(task_id, name)
            records[record.filename] = record

        for record in records.itervalues():
            self.log(logging.INFO, 'artifact', {'name': record.basename},
                     'Downloading {name}')
            valid = False
            # sleeptime is 60 per retry.py, used by tooltool_wrapper.sh
            for attempt, _ in enumerate(redo.retrier(attempts=retry+1,
                                                     sleeptime=60)):
                try:
                    record.fetch_with(cache)
                except (requests.exceptions.HTTPError,
                        requests.exceptions.ChunkedEncodingError,
                        requests.exceptions.ConnectionError) as e:

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
                    # e.message is not always a string, so convert it first.
                    self.log(level, 'artifact', {}, str(e.message))
                    if not should_retry:
                        break
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Will retry in a moment...')
                    continue
                try:
                    valid = record.validate()
                except Exception:
                    pass
                if not valid:
                    os.unlink(record.filename)
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Corrupt download. Will retry in a moment...')
                    continue

                downloaded.append(record)
                break

            if not valid:
                self.log(logging.ERROR, 'artifact', {'name': record.basename},
                         'Failed to download {name}')
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
                with open(local) as fh:
                    h = hashlib.sha256()
                    while True:
                        data = fh.read(1024 * 1024)
                        if not data:
                            break
                        h.update(data)
                artifacts[record.url] = {
                    'sha256': h.hexdigest(),
                }
            if record.unpack and not no_unpack:
                unpack_file(local)
                os.unlink(local)

        if not downloaded:
            self.log(logging.ERROR, 'artifact', {}, 'Nothing to download')
            if files:
                return 1

        if artifacts:
            ensureParentDir(artifact_manifest)
            with open(artifact_manifest, 'w') as fh:
                json.dump(artifacts, fh, indent=4, sort_keys=True)

        return 0
