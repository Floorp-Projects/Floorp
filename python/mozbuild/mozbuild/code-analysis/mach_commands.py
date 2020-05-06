# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

import concurrent.futures
import io
import logging
import json
import multiprocessing
import ntpath
import os
import re
import sys
import subprocess
import shutil
import tarfile
import tempfile
import xml.etree.ElementTree as ET
import yaml

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mach.main import Mach

from mozbuild.base import MachCommandBase

from mozbuild.build_commands import Build
from mozbuild.nodeutil import find_node_executable

import mozpack.path as mozpath

from mozversioncontrol import get_repository_object

from mozbuild.controller.clobber import Clobberer


# Function used to run clang-format on a batch of files. It is a helper function
# in order to integrate into the futures ecosystem clang-format.
def run_one_clang_format_batch(args):
    try:
        subprocess.check_output(args)
    except subprocess.CalledProcessError as e:
        return e


def build_repo_relative_path(abs_path, repo_path):
    '''Build path relative to repository root'''

    if os.path.islink(abs_path):
        abs_path = mozpath.realpath(abs_path)

    return mozpath.relpath(abs_path, repo_path)


def prompt_bool(prompt, limit=5):
    ''' Prompts the user with prompt and requires a boolean value. '''
    from distutils.util import strtobool

    for _ in range(limit):
        try:
            return strtobool(raw_input(prompt + "[Y/N]\n"))
        except ValueError:
            print("ERROR! Please enter a valid option! Please use any of the following:"
                  " Y, N, True, False, 1, 0")
    return False


class StaticAnalysisSubCommand(SubCommand):
    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        args = [
            CommandArgument('--verbose', '-v', action='store_true',
                            help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


class StaticAnalysisMonitor(object):
    def __init__(self, srcdir, objdir, clang_tidy_config, total):
        self._total = total
        self._processed = 0
        self._current = None
        self._srcdir = srcdir

        self._clang_tidy_config = clang_tidy_config['clang_checkers']
        # Transform the configuration to support Regex
        for item in self._clang_tidy_config:
            if item['name'] == '-*':
                continue
            item['name'] = item['name'].replace('*', '.*')

        from mozbuild.compilation.warnings import (
            WarningsCollector,
            WarningsDatabase,
        )

        self._warnings_database = WarningsDatabase()

        def on_warning(warning):

            # Output paths relative to repository root if the paths are under repo tree
            warning['filename'] = build_repo_relative_path(warning['filename'], self._srcdir)

            self._warnings_database.insert(warning)

        self._warnings_collector = WarningsCollector(on_warning, objdir=objdir)

    @property
    def num_files(self):
        return self._total

    @property
    def num_files_processed(self):
        return self._processed

    @property
    def current_file(self):
        return self._current

    @property
    def warnings_db(self):
        return self._warnings_database

    def on_line(self, line):
        warning = None

        try:
            warning = self._warnings_collector.process_line(line)
        except Exception:
            pass

        if line.find('clang-tidy') != -1:
            filename = line.split(' ')[-1]
            if os.path.isfile(filename):
                self._current = build_repo_relative_path(filename, self._srcdir)
            else:
                self._current = None
            self._processed = self._processed + 1
            return (warning, False)
        if warning is not None:
            def get_check_config(checker_name):
                # get the matcher from self._clang_tidy_config that is the 'name' field
                for item in self._clang_tidy_config:
                    if item['name'] == checker_name:
                        return item

                    # We are using a regex in order to also match 'mozilla-.* like checkers'
                    matcher = re.match(item['name'], checker_name)
                    if matcher is not None and matcher.group(0) == checker_name:
                        return item

            check_config = get_check_config(warning['flag'])
            if check_config is not None:
                warning['reliability'] = check_config.get('reliability', 'low')
                warning['reason'] = check_config.get('reason')
                warning['publish'] = check_config.get('publish', True)
        return (warning, True)


@CommandProvider
class StaticAnalysis(MachCommandBase):
    """Utilities for running C++ static analysis checks and format."""

    # List of file extension to consider (should start with dot)
    _format_include_extensions = ('.cpp', '.c', '.cc', '.h', '.m', '.mm')
    # File contaning all paths to exclude from formatting
    _format_ignore_file = '.clang-format-ignore'

    # List of file extension to consider (should start with dot)
    _check_syntax_include_extensions = ('.cpp', '.c', '.cc', '.cxx')

    _clang_tidy_config = None
    _cov_config = None

    @Command('static-analysis', category='testing',
             description='Run C++ static analysis checks')
    def static_analysis(self):
        # If no arguments are provided, just print a help message.
        """Detailed documentation:
        https://firefox-source-docs.mozilla.org/code-quality/static-analysis.html
        """
        mach = Mach(os.getcwd())

        def populate_context(context, key=None):
            context.topdir = self.topsrcdir

        mach.populate_context_handler = populate_context
        mach.run(['static-analysis', '--help'])

    @StaticAnalysisSubCommand('static-analysis', 'check',
                              'Run the checks using the helper tool')
    @CommandArgument('source', nargs='*', default=['.*'],
                     help='Source files to be analyzed (regex on path). '
                          'Can be omitted, in which case the entire code base '
                          'is analyzed.  The source argument is ignored if '
                          'there is anything fed through stdin, in which case '
                          'the analysis is only performed on the files changed '
                          'in the patch streamed through stdin.  This is called '
                          'the diff mode.')
    @CommandArgument('--checks', '-c', default='-*', metavar='checks',
                     help='Static analysis checks to enable.  By default, this enables only '
                     'checks that are published here: https://mzl.la/2DRHeTh, but can be any '
                     'clang-tidy checks syntax.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
                     help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('--strip', '-p', default='1', metavar='NUM',
                     help='Strip NUM leading components from file names in diff mode.')
    @CommandArgument('--fix', '-f', default=False, action='store_true',
                     help='Try to autofix errors detected by clang-tidy checkers.')
    @CommandArgument('--header-filter', '-h-f', default='', metavar='header_filter',
                     help='Regular expression matching the names of the headers to '
                          'output diagnostics from. Diagnostics from the main file '
                          'of each translation unit are always displayed')
    @CommandArgument('--output', '-o', default=None,
                     help='Write clang-tidy output in a file')
    @CommandArgument('--format', default='text', choices=('text', 'json'),
                     help='Output format to write in a file')
    @CommandArgument('--outgoing', default=False, action='store_true',
                     help='Run static analysis checks on outgoing files from mercurial repository')
    def check(self, source=None, jobs=2, strip=1, verbose=False, checks='-*',
              fix=False, header_filter='', output=None, format='text', outgoing=False):
        from mozbuild.controller.building import (
            StaticAnalysisFooter,
            StaticAnalysisOutputManager,
        )

        self._set_log_level(verbose)
        self._activate_virtualenv()
        self.log_manager.enable_unstructured()

        rc = self._get_clang_tools(verbose=verbose)
        if rc != 0:
            return rc

        if self._is_version_eligible() is False:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: You're using an old version of clang-format binary."
                     " Please update to a more recent one by running: './mach bootstrap'")
            return 1

        rc = self._build_compile_db(verbose=verbose)
        rc = rc or self._build_export(jobs=jobs, verbose=verbose)
        if rc != 0:
            return rc

        # Use outgoing files instead of source files
        if outgoing:
            repo = get_repository_object(self.topsrcdir)
            files = repo.get_outgoing_files()
            source = map(os.path.abspath, files)

        # Split in several chunks to avoid hitting Python's limit of 100 groups in re
        compile_db = json.loads(open(self._compile_db, 'r').read())
        total = 0
        import re
        chunk_size = 50
        for offset in range(0, len(source), chunk_size):
            source_chunks = source[offset:offset + chunk_size]
            name_re = re.compile('(' + ')|('.join(source_chunks) + ')')
            for f in compile_db:
                if name_re.search(f['file']):
                    total = total + 1

        # Filter source to remove excluded files
        source = self._generate_path_list(source, verbose=verbose)

        if not total or not source:
            self.log(logging.INFO, 'static-analysis', {},
                     "There are no files eligible for analysis. Please note that 'header' files "
                     "cannot be used for analysis since they do not consist compilation units.")
            return 0

        cwd = self.topobjdir
        self._compilation_commands_path = self.topobjdir
        if self._clang_tidy_config is None:
            self._clang_tidy_config = self._get_clang_tidy_config()

        monitor = StaticAnalysisMonitor(
            self.topsrcdir, self.topobjdir, self._clang_tidy_config, total)

        footer = StaticAnalysisFooter(self.log_manager.terminal, monitor)

        with StaticAnalysisOutputManager(self.log_manager, monitor, footer) as output_manager:
            import math
            batch_size = int(math.ceil(float(len(source)) / multiprocessing.cpu_count()))
            for i in range(0, len(source), batch_size):
                args = self._get_clang_tidy_command(
                    checks=checks,
                    header_filter=header_filter,
                    sources=source[i:(i + batch_size)],
                    jobs=jobs,
                    fix=fix)
                rc = self.run_process(
                    args=args,
                    ensure_exit_code=False,
                    line_handler=output_manager.on_line,
                    cwd=cwd)

            self.log(logging.WARNING, 'warning_summary',
                     {'count': len(monitor.warnings_db)},
                     '{count} warnings present.')

            # Write output file
            if output is not None:
                output_manager.write(output, format)

        if rc != 0:
            return rc
        # if we are building firefox for android it might be nice to
        # also analyze the java code base
        if self.substs['MOZ_BUILD_APP'] == 'mobile/android':
            rc = self.check_java(source, jobs, strip, verbose, skip_export=True)
        return rc

    @StaticAnalysisSubCommand('static-analysis', 'check-coverity',
                              'Run coverity static-analysis tool on the given files. '
                              'Can only be run by automation! '
                              'It\'s result is stored as an json file on the artifacts server.')
    @CommandArgument('source', nargs='*', default=[],
                     help='Source files to be analyzed by Coverity Static Analysis Tool. '
                          'This is ran only in automation.')
    @CommandArgument('--output', '-o', default=None,
                     help='Write coverity output translated to json output in a file')
    @CommandArgument('--coverity_output_path', '-co', default=None,
                     help='Path where to write coverity results as cov-results.json. '
                     'If no path is specified the default path from the coverity working '
                     'directory, ~./mozbuild/coverity is used.')
    @CommandArgument('--outgoing', default=False, action='store_true',
                     help='Run coverity on outgoing files from mercurial or git repository')
    @CommandArgument('--full-build', default=False, action='store_true',
                     help='Run a full build for coverity analisys.')
    def check_coverity(self, source=[], output=None, coverity_output_path=None,
                       outgoing=False, full_build=False, verbose=False):
        self._set_log_level(verbose)
        self._activate_virtualenv()
        self.log_manager.enable_unstructured()

        if 'MOZ_AUTOMATION' not in os.environ:
            self.log(logging.INFO, 'static-analysis', {},
                     'Coverity based static-analysis cannot be ran outside automation.')
            return

        if full_build and outgoing:
            self.log(logging.INFO, 'static-analysis', {},
                     'Coverity full build cannot be associated with outgoing.')
            return

        # Use outgoing files instead of source files
        if outgoing:
            repo = get_repository_object(self.topsrcdir)
            files = repo.get_outgoing_files()
            source = map(os.path.abspath, files)

        # Verify that we have source files or we are dealing with a full-build
        if len(source) == 0 and not full_build:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: There are no files that coverity can use to scan.')
            return 0

        # Load the configuration file for coverity static-analysis
        # For the moment we store only the reliability index for each checker
        # as the rest is managed on the https://github.com/mozilla/release-services side.
        self._cov_config = self._get_cov_config()

        rc = self.setup_coverity()
        if rc != 0:
            return rc

        # First run cov-run-desktop --setup in order to setup the analysis env
        # We need this in both cases, per patch analysis or full tree build
        cmd = [self.cov_run_desktop, '--setup']
        if self.run_cov_command(cmd, self.cov_path):
            # Avoiding a bug in Coverity where snapshot is not identified
            # as beeing built with the current analysis binary.
            if not full_build:
                return 1

        # Run cov-configure for clang, javascript and python
        langs = ["clang", "javascript", "python"]
        for lang in langs:
            cmd = [self.cov_configure, '--{}'.format(lang)]

            if self.run_cov_command(cmd):
                return 1

        if full_build:
            # 1. Build the model file that is going to be used for analysis
            model_path = mozpath.join("tools", "coverity", "model.cpp")
            cmd = [self.cov_make_library, "-sf", self.cov_lic_path, model_path]

            if self.run_cov_command(cmd):
                return 1

            # 2. Run cov-build

            # Add cov_build command
            cmd = [
                self.cov_build,
                '--dir',
                'cov-int'
            ]
            # Add fs capture search paths for languages that are not nuilt
            cmd += [
                "--fs-capture-search={}".format(path)
                for path in self.cov_capture_search
            ]

            # Add the exclude criteria for test cases
            cmd += [
                '--fs-capture-search-exclude-regex',
                '.*/test',
                './mach', '--log-no-times', 'build'
            ]
            if self.run_cov_command(cmd):
                return 1

            # 3. Run cov-analyze and exclude disabled checkers
            cmd = [
                self.cov_analyze,
                '--dir',
                'cov-int',
                '--all',
                '--enable-virtual',
                '--strip-path={}'.format(self.topsrcdir),
                '-sf',
                self.cov_lic_path
            ]

            cmd += [
                "--disable={}".format(key)
                for key, checker in self._cov_config['coverity_checkers'].items()
                if checker.get("publish", True) is False
            ]

            if self.run_cov_command(cmd):
                return 1

            # 4. Run cov-commit-defects
            protocol = "https" if self.cov_server_ssl else "http"
            server_url = "{0}://{1}:{2}".format(protocol, self.cov_url, self.cov_port)
            cmd = [
                self.cov_commit_defects,
                "--auth-key-file",
                self.cov_auth_path,
                "--stream",
                self.cov_stream,
                "--dir",
                "cov-int",
                "--url",
                server_url,
                "-sf",
                self.cov_lic_path
            ]

            if self.run_cov_command(cmd):
                return 1

            return 0

        rc = self._build_compile_db(verbose=verbose)
        rc = rc or self._build_export(jobs=2, verbose=verbose)

        if rc != 0:
            return rc

        commands_list = self.get_files_with_commands(source)
        if len(commands_list) == 0:
            self.log(logging.INFO, 'static-analysis', {},
                     'There are no files that need to be analyzed.')
            return 0

        if len(self.cov_non_unified_paths):
            self.cov_non_unified_paths = [mozpath.join(
                self.topsrcdir, path) for path in self.cov_non_unified_paths]

        # For each element in commands_list run `cov-translate`
        for element in commands_list:

            def transform_cmd(cmd):
                # Coverity Analysis has a problem translating definitions passed as:
                # '-DSOME_DEF="ValueOfAString"', please see Bug 1588283.
                return [re.sub(r'\'-D(.*)="(.*)"\'', r'-D\1="\2"', arg) for arg in cmd]

            build_command = element['command'].split(' ')
            # For modules that are compatible with the non unified build environment
            # use the the implicit file for analysis in the detriment of the unified
            if any(element['file'].startswith(path) for path in self.cov_non_unified_paths):
                build_command[-1] = element['file']

            cmd = [self.cov_translate, '--dir', self.cov_idir_path] + \
                transform_cmd(build_command)

            if self.run_cov_command(cmd, element['directory']):
                return 1

        if coverity_output_path is None:
            cov_result = mozpath.join(self.cov_state_path, 'cov-results.json')
        else:
            cov_result = mozpath.join(coverity_output_path, 'cov-results.json')

        # Once the capture is performed we need to do the actual Coverity Desktop analysis
        cmd = [self.cov_run_desktop, '--json-output-v6',
               cov_result, '--analyze-captured-source']

        if self.run_cov_command(cmd, self.cov_state_path):
            return 1

        if output is not None:
            self.dump_cov_artifact(cov_result, source, output)

    def run_cov_command(self, cmd, path=None):
        if path is None:
            # We want to run it in topsrcdir
            path = self.topsrcdir

        self.log(logging.INFO, 'static-analysis', {}, 'Running '+' '.join(cmd))

        rc = self.run_process(args=cmd, cwd=path, pass_thru=True)

        if rc != 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Running ' + ' '.join(cmd) + ' failed!')
            return rc
        return 0

    def get_reliability_index_for_cov_checker(self, checker_name):
        if self._cov_config is None:
            self.log(logging.INFO, 'static-analysis', {}, 'Coverity config file not found, '
                     'using default-value \'reliablity\' = medium. for checker {}'.format(
                        checker_name))
            return 'medium'

        checkers = self._cov_config['coverity_checkers']
        if checker_name not in checkers:
            self.log(logging.INFO, 'static-analysis', {},
                     'Coverity checker {} not found to determine reliability index. '
                     'For the moment we shall use the default \'reliablity\' = medium.'.format(
                        checker_name))
            return 'medium'

        if 'reliability' not in checkers[checker_name]:
            # This checker doesn't have a reliability index
            self.log(logging.INFO, 'static-analysis', {},
                     'Coverity checker {} doesn\'t have a reliability index set, '
                     'field \'reliability is missing\', please cosinder adding it. '
                     'For the moment we shall use the default \'reliablity\' = medium.'.format(
                        checker_name))
            return 'medium'

        return checkers[checker_name]['reliability']

    def dump_cov_artifact(self, cov_results, source, output):
        # Parse Coverity json into structured issues

        with open(cov_results) as f:
            result = json.load(f)

            # Parse the issues to a standard json format
            issues_dict = {'files': {}}

            files_list = issues_dict['files']

            def build_element(issue):
                # We look only for main event
                event_path = next(
                    (event for event in issue['events'] if event['main'] is True), None)

                dict_issue = {
                    'line': issue['mainEventLineNumber'],
                    'flag': issue['checkerName'],
                    'build_error': issue['checkerName'].startswith('RW.CLANG'),
                    'message': event_path['eventDescription'],
                    'reliability': self.get_reliability_index_for_cov_checker(
                        issue['checkerName']
                        ),
                    'extra': {
                        'category': issue['checkerProperties']['category'],
                        'stateOnServer': issue['stateOnServer'],
                        'stack': []
                    }
                }

                # Embed all events into extra message
                for event in issue['events']:
                    dict_issue['extra']['stack'].append(
                        {'file_path': build_repo_relative_path(event['strippedFilePathname'],
                                                               self.topsrcdir),
                         'line_number': event['lineNumber'],
                         'path_type': event['eventTag'],
                         'description': event['eventDescription']})

                return dict_issue

            for issue in result['issues']:
                path = build_repo_relative_path(issue['strippedMainEventFilePathname'],
                                                self.topsrcdir)
                if path is None:
                    # Since we skip a result we should log it
                    self.log(logging.INFO, 'static-analysis', {},
                             'Skipping CID: {0} from file: {1} since it\'s not related '
                             'with the current patch.'.format(
                                issue['stateOnServer']['cid'],
                                issue['strippedMainEventFilePathname'])
                             )
                    continue
                if path in files_list:
                    files_list[path]['warnings'].append(build_element(issue))
                else:
                    files_list[path] = {'warnings': [build_element(issue)]}

            with open(output, 'w') as f:
                json.dump(issues_dict, f)

    def get_coverity_secrets(self):
        from taskgraph.util.taskcluster import get_root_url

        secret_name = 'project/relman/coverity'
        secrets_url = '{}/secrets/v1/secret/{}'.format(get_root_url(True), secret_name)

        self.log(logging.INFO, 'static-analysis', {},
                 'Using symbol upload token from the secrets service: "{}"'.format(secrets_url))

        import requests
        res = requests.get(secrets_url)
        res.raise_for_status()
        secret = res.json()
        cov_config = secret['secret'] if 'secret' in secret else None

        if cov_config is None:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Ill formatted secret for Coverity. Aborting analysis.')
            return 1

        self.cov_analysis_url = cov_config.get('package_url')
        self.cov_package_name = cov_config.get('package_name')
        self.cov_url = cov_config.get('server_url')
        self.cov_server_ssl = cov_config.get('server_ssl', True)
        # In case we don't have a port in the secret we use the default one,
        # for a default coverity deployment.
        self.cov_port = cov_config.get('server_port', 8443)
        self.cov_auth = cov_config.get('auth_key')
        self.cov_package_ver = cov_config.get('package_ver')
        self.cov_lic_name = cov_config.get('lic_name')
        self.cov_capture_search = cov_config.get('fs_capture_search', None)
        self.cov_full_stack = cov_config.get('full_stack', False)
        self.cov_stream = cov_config.get('stream', False)
        self.cov_non_unified_paths = cov_config.get('non_unified', [])

        return 0

    def download_coverity(self):
        if self.cov_url is None or self.cov_port is None or \
                self.cov_analysis_url is None or \
                self.cov_auth is None:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Missing Coverity secret on try job!')
            return 1

        COVERITY_CONFIG = '''
        {
            "type": "Coverity configuration",
            "format_version": 1,
            "settings": {
            "server": {
                "host": "%s",
                "ssl" : true,
                "port": %s,
                "on_new_cert" : "trust",
                "auth_key_file": "%s"
            },
            "stream": "Firefox",
            "cov_run_desktop": {
                "build_cmd": [],
                "clean_cmd": []
            }
            }
        }
        '''
        # Generate the coverity.conf and auth files
        self.cov_auth_path = mozpath.join(self.cov_state_path, 'auth')
        cov_setup_path = mozpath.join(self.cov_state_path, 'coverity.conf')
        cov_conf = COVERITY_CONFIG % (self.cov_url, self.cov_port, self.cov_auth_path)

        def download(artifact_url, target):
            import requests
            self.log_manager.enable_unstructured()
            resp = requests.get(artifact_url, verify=False, stream=True)
            self.log_manager.disable_unstructured()
            resp.raise_for_status()

            # Extract archive into destination
            with tarfile.open(fileobj=io.BytesIO(resp.content)) as tar:
                tar.extractall(target)

        download(self.cov_analysis_url, self.cov_state_path)

        with open(self.cov_auth_path, 'w') as f:
            f.write(self.cov_auth)

        # Modify it's permission to 600
        os.chmod(self.cov_auth_path, 0o600)

        with open(cov_setup_path, 'a') as f:
            f.write(cov_conf)

    def setup_coverity(self, force_download=True):
        rc, config, _ = self._get_config_environment()
        rc = rc or self.get_coverity_secrets()

        if rc != 0:
            return rc

        # Create a directory in mozbuild where we setup coverity
        self.cov_state_path = mozpath.join(self._mach_context.state_dir, "coverity")

        if force_download is True and os.path.exists(self.cov_state_path):
            shutil.rmtree(self.cov_state_path)

        os.mkdir(self.cov_state_path)

        # Download everything that we need for Coverity from out private instance
        self.download_coverity()

        self.cov_path = mozpath.join(self.cov_state_path, self.cov_package_name)
        self.cov_run_desktop = mozpath.join(self.cov_path, 'bin', 'cov-run-desktop')
        self.cov_configure = mozpath.join(self.cov_path, 'bin', 'cov-configure')
        self.cov_make_library = mozpath.join(self.cov_path, 'bin', 'cov-make-library')
        self.cov_build = mozpath.join(self.cov_path, 'bin', 'cov-build')
        self.cov_analyze = mozpath.join(self.cov_path, 'bin', 'cov-analyze')
        self.cov_commit_defects = mozpath.join(self.cov_path, 'bin', 'cov-commit-defects')
        self.cov_translate = mozpath.join(self.cov_path, 'bin', 'cov-translate')
        self.cov_configure = mozpath.join(self.cov_path, 'bin', 'cov-configure')
        self.cov_work_path = mozpath.join(self.cov_state_path, 'data-coverity')
        self.cov_idir_path = mozpath.join(self.cov_work_path, self.cov_package_ver, 'idir')
        self.cov_lic_path = mozpath.join(
            self.cov_work_path, self.cov_package_ver, 'lic', self.cov_lic_name)

        if not os.path.exists(self.cov_path):
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Missing Coverity in {}'.format(self.cov_path))
            return 1

        return 0

    def get_files_with_commands(self, source):
        '''
        Returns an array of dictionaries having file_path with build command
        '''

        compile_db = json.load(open(self._compile_db, 'r'))

        commands_list = []

        for f in source:
            # It must be a C/C++ file
            _, ext = os.path.splitext(f)

            if ext.lower() not in self._format_include_extensions:
                self.log(logging.INFO, 'static-analysis', {}, 'Skipping {}'.format(f))
                continue
            file_with_abspath = os.path.join(self.topsrcdir, f)
            for f in compile_db:
                # Found for a file that we are looking
                if file_with_abspath == f['file']:
                    commands_list.append(f)

        return commands_list

    @StaticAnalysisSubCommand('static-analysis', 'check-java',
                              'Run infer on the java codebase.')
    @CommandArgument('source', nargs='*', default=['mobile'],
                     help='Source files to be analyzed. '
                          'Can be omitted, in which case the entire code base '
                          'is analyzed.  The source argument is ignored if '
                          'there is anything fed through stdin, in which case '
                          'the analysis is only performed on the files changed '
                          'in the patch streamed through stdin.  This is called '
                          'the diff mode.')
    @CommandArgument('--checks', '-c', default=[], metavar='checks', nargs='*',
                     help='Static analysis checks to enable.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
                     help='Number of concurrent jobs to run.'
                     ' Default is the number of CPUs.')
    @CommandArgument('--task', '-t', type=str,
                     default='compileWithGeckoBinariesDebugSources',
                     help='Which gradle tasks to use to compile the java codebase.')
    @CommandArgument('--outgoing', default=False, action='store_true',
                     help='Run infer checks on outgoing files from repository')
    @CommandArgument('--output', default=None,
                     help='Write infer json output in a file')
    def check_java(self, source=['mobile'], jobs=2, strip=1, verbose=False, checks=[],
                   task='compileWithGeckoBinariesDebugSources',
                   skip_export=False, outgoing=False, output=None):
        self._set_log_level(verbose)
        self._activate_virtualenv()
        self.log_manager.enable_unstructured()

        if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
            self.log(logging.WARNING, 'static-analysis', {},
                     'Cannot check java source code unless you are building for android!')
            return 1
        rc = self._check_for_java()
        if rc != 0:
            return 1
        if output is not None:
            output = os.path.abspath(output)
            if not os.path.isdir(os.path.dirname(output)):
                self.log(logging.WARNING, 'static-analysis', {},
                         'Missing report destination folder for {}'.format(output))

        # if source contains the whole mobile folder, then we just have to
        # analyze everything
        check_all = any(i.rstrip(os.sep).split(os.sep)[-1] == 'mobile' for i in source)
        # gather all java sources from the source variable
        java_sources = []
        if outgoing:
            repo = get_repository_object(self.topsrcdir)
            java_sources = self._get_java_files(repo.get_outgoing_files())
            if not java_sources:
                self.log(logging.WARNING, 'static-analysis', {},
                         'No outgoing Java files to check')
                return 0
        elif not check_all:
            java_sources = self._get_java_files(source)
            if not java_sources:
                return 0
        if not skip_export:
            rc = self._build_export(jobs=jobs, verbose=verbose)
            if rc != 0:
                return rc
        rc = self._get_infer(verbose=verbose)
        if rc != 0:
            self.log(logging.WARNING, 'static-analysis', {},
                     'This command is only available for linux64!')
            return rc
        # which checkers to use, and which folders to exclude
        all_checkers, third_party_path, generated_path = self._get_infer_config()
        checkers, excludes = self._get_infer_args(checks or all_checkers, third_party_path,
                                                  generated_path)
        rc = rc or self._gradle(['clean'])  # clean so that we can recompile
        # infer capture command
        capture_cmd = [self._infer_path, 'capture'] + excludes + ['--']
        rc = rc or self._gradle([task], infer_args=capture_cmd, verbose=verbose)
        tmp_file, args = self._get_infer_source_args(java_sources)
        # infer analyze command
        analysis_cmd = [self._infer_path, 'analyze', '--keep-going'] +  \
            checkers + args
        rc = rc or self.run_process(args=analysis_cmd, cwd=self.topsrcdir, pass_thru=True)
        if tmp_file:
            tmp_file.close()

        # Copy the infer report
        report_path = os.path.join(self.topsrcdir, 'infer-out', 'report.json')
        if output is not None and os.path.exists(report_path):
            shutil.copy(report_path, output)
            self.log(logging.INFO, 'static-analysis', {},
                     'Report available in {}'.format(output))

        return rc

    def _get_java_files(self, sources):
        java_sources = []
        for i in sources:
            f = mozpath.join(self.topsrcdir, i)
            if os.path.isdir(f):
                for root, dirs, files in os.walk(f):
                    dirs.sort()
                    for file in sorted(files):
                        if file.endswith('.java'):
                            java_sources.append(mozpath.join(root, file))
            elif f.endswith('.java'):
                java_sources.append(f)
        return java_sources

    def _get_infer_source_args(self, sources):
        '''Return the arguments to only analyze <sources>'''
        if not sources:
            return (None, [])
        # create a temporary file in which we place all sources
        # this is used by the analysis command to only analyze certain files
        f = tempfile.NamedTemporaryFile()
        for source in sources:
            f.write(source+'\n')
        f.flush()
        return (f, ['--changed-files-index', f.name])

    def _get_infer_config(self):
        '''Load the infer config file.'''
        checkers = []
        tp_path = ''
        with open(mozpath.join(self.topsrcdir, 'tools',
                               'infer', 'config.yaml')) as f:
            try:
                config = yaml.safe_load(f)
                for item in config['infer_checkers']:
                    if item['publish']:
                        checkers.append(item['name'])
                tp_path = mozpath.join(self.topsrcdir, config['third_party'])
                generated_path = mozpath.join(self.topsrcdir, config['generated'])
            except Exception:
                print('Looks like config.yaml is not valid, so we are unable '
                      'to determine default checkers, and which folder to '
                      'exclude, using defaults provided by infer')
        return checkers, tp_path, generated_path

    def _get_infer_args(self, checks, *input_paths):
        '''Return the arguments which include the checkers <checks>, and
        excludes all folder in <third_party_path>.'''
        checkers = ['-a', 'checkers']
        excludes = []
        for checker in checks:
            checkers.append('--' + checker)
        for path in input_paths:
            with open(path) as f:
                for line in f:
                    excludes.append('--skip-analysis-in-path')
                    excludes.append(line.strip('\n'))
        return checkers, excludes

    def _get_clang_tidy_config(self):
        try:
            file_handler = open(mozpath.join(self.topsrcdir, "tools", "clang-tidy", "config.yaml"))
            config = yaml.safe_load(file_handler)
        except Exception:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Looks like config.yaml is not valid, we are going to use default'
                     ' values for the rest of the analysis for clang-tidy.')
            return None
        return config

    def _get_cov_config(self):
        try:
            file_handler = open(mozpath.join(self.topsrcdir, "tools", "coverity", "config.yaml"))
            config = yaml.safe_load(file_handler)
        except Exception:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Looks like config.yaml is not valid, we are going to use default'
                     ' values for the rest of the analysis for coverity.')
            return None
        return config

    def _is_version_eligible(self):
        # make sure that we've cached self._clang_tidy_config
        if self._clang_tidy_config is None:
            self._clang_tidy_config = self._get_clang_tidy_config()

        version = None
        if 'package_version' in self._clang_tidy_config:
            version = self._clang_tidy_config['package_version']
        else:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: Unable to find 'package_version' in the config.yml")
            return False

        # Because the fact that we ship together clang-tidy and clang-format
        # we are sure that these two will always share the same version.
        # Thus in order to determine that the version is compatible we only
        # need to check one of them, going with clang-format
        cmd = [self._clang_format_path, '--version']
        try:
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')
            version_string = 'clang-format version ' + version
            if output.startswith(version_string):
                return True
        except subprocess.CalledProcessError as e:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: Error determining the version clang-tidy/format binary, "
                     "please see the attached exception: \n{}".format(e.output))
        return False

    def _get_clang_tidy_command(self, checks, header_filter, sources, jobs, fix):

        if checks == '-*':
            checks = self._get_checks()

        common_args = ['-clang-tidy-binary', self._clang_tidy_path,
                       '-clang-apply-replacements-binary', self._clang_apply_replacements,
                       '-checks=%s' % checks,
                       '-extra-arg=-std=c++17', '-extra-arg=-DMOZ_CLANG_PLUGIN']

        # Flag header-filter is passed in order to limit the diagnostic messages only
        # to the specified header files. When no value is specified the default value
        # is considered to be the source in order to limit the diagnostic message to
        # the source files or folders.
        common_args += ['-header-filter=%s' % (header_filter
                                               if len(header_filter) else '|'.join(sources))]

        # From our configuration file, config.yaml, we build the configuration list, for
        # the checkers that are used. These configuration options are used to better fit
        # the checkers to our code.
        cfg = self._get_checks_config()
        if cfg:
            common_args += ['-config=%s' % yaml.dump(cfg)]

        if fix:
            common_args += ['-fix']

        return [
            self.virtualenv_manager.python_path, self._run_clang_tidy_path, '-j',
            str(jobs), '-p', self._compilation_commands_path
        ] + common_args + sources

    def _check_for_java(self):
        '''Check if javac can be found.'''
        import distutils
        java = self.substs.get('JAVA')
        java = java or os.getenv('JAVA_HOME')
        java = java or distutils.spawn.find_executable('javac')
        error = 'javac was not found! Please install javac and either add it to your PATH, '
        error += 'set JAVA_HOME, or add the following to your mozconfig:\n'
        error += '  --with-java-bin-path=/path/to/java/bin/'
        if not java:
            self.log(logging.ERROR, 'ERROR: static-analysis', {}, error)
            return 1
        return 0

    def _gradle(self, args, infer_args=None, verbose=False, autotest=False,
                suppress_output=True):
        infer_args = infer_args or []
        if autotest:
            cwd = mozpath.join(self.topsrcdir, 'tools', 'infer', 'test')
            gradle = mozpath.join(cwd, 'gradlew')
        else:
            gradle = self.substs['GRADLE']
            cwd = self.topsrcdir
        extra_env = {
            'GRADLE_OPTS': '-Dfile.encoding=utf-8',  # see mobile/android/mach_commands.py
            'JAVA_TOOL_OPTIONS': '-Dfile.encoding=utf-8',
        }
        if suppress_output:
            devnull = open(os.devnull, 'w')
            return subprocess.call(
                infer_args + [gradle] + args,
                env=dict(os.environ, **extra_env),
                cwd=cwd, stdout=devnull, stderr=subprocess.STDOUT, close_fds=True)

        return self.run_process(
            infer_args + [gradle] + args,
            append_env=extra_env,
            pass_thru=True,  # Allow user to run gradle interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=cwd)

    @StaticAnalysisSubCommand('static-analysis', 'autotest',
                              'Run the auto-test suite in order to determine that'
                              ' the analysis did not regress.')
    @CommandArgument('--dump-results', '-d', default=False, action='store_true',
                     help='Generate the baseline for the regression test. Based on'
                     ' this baseline we will test future results.')
    @CommandArgument('--intree-tool', '-i', default=False, action='store_true',
                     help='Use a pre-aquired in-tree clang-tidy package from the automation env.'
                     ' This option is only valid on automation environments.')
    @CommandArgument('checker_names', nargs='*', default=[],
                     help='Checkers that are going to be auto-tested.')
    def autotest(self, verbose=False, dump_results=False, intree_tool=False, checker_names=[]):
        # If 'dump_results' is True than we just want to generate the issues files for each
        # checker in particulat and thus 'force_download' becomes 'False' since we want to
        # do this on a local trusted clang-tidy package.
        self._set_log_level(verbose)
        self._activate_virtualenv()
        self._dump_results = dump_results

        force_download = not self._dump_results

        # Function return codes
        self.TOOLS_SUCCESS = 0
        self.TOOLS_FAILED_DOWNLOAD = 1
        self.TOOLS_UNSUPORTED_PLATFORM = 2
        self.TOOLS_CHECKER_NO_TEST_FILE = 3
        self.TOOLS_CHECKER_RETURNED_NO_ISSUES = 4
        self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND = 5
        self.TOOLS_CHECKER_DIFF_FAILED = 6
        self.TOOLS_CHECKER_NOT_FOUND = 7
        self.TOOLS_CHECKER_FAILED_FILE = 8
        self.TOOLS_CHECKER_LIST_EMPTY = 9
        self.TOOLS_GRADLE_FAILED = 10

        # Configure the tree or download clang-tidy package, depending on the option that we choose
        if intree_tool:
            if 'MOZ_AUTOMATION' not in os.environ:
                self.log(logging.INFO, 'static-analysis', {},
                         'The `autotest` with `--intree-tool` can only be ran in automation.')
                return 1
            if 'MOZ_FETCHES_DIR' not in os.environ:
                self.log(logging.INFO, 'static-analysis', {},
                         '`MOZ_FETCHES_DIR` is missing from the environment variables.')
                return 1

            _, config, _ = self._get_config_environment()
            clang_tools_path = os.environ['MOZ_FETCHES_DIR']
            self._clang_tidy_path = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-tidy" + config.substs.get('BIN_SUFFIX', ''))
            self._clang_format_path = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-format" + config.substs.get('BIN_SUFFIX', ''))
            self._clang_apply_replacements = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-apply-replacements" + config.substs.get('BIN_SUFFIX', ''))
            self._run_clang_tidy_path = mozpath.join(clang_tools_path, "clang-tidy", "share",
                                                     "clang", "run-clang-tidy.py")
            self._clang_format_diff = mozpath.join(clang_tools_path, "clang-tidy", "share",
                                                   "clang", "clang-format-diff.py")

            # Ensure that clang-tidy is present
            rc = not os.path.exists(self._clang_tidy_path)
        else:
            rc = self._get_clang_tools(force=force_download, verbose=verbose)

        if rc != 0:
            self.log(logging.ERROR, 'ERROR: static-analysis', {},
                     'ERROR: clang-tidy unable to locate package.')
            return self.TOOLS_FAILED_DOWNLOAD

        self._clang_tidy_base_path = mozpath.join(self.topsrcdir, "tools", "clang-tidy")

        # For each checker run it
        self._clang_tidy_config = self._get_clang_tidy_config()
        platform, _ = self.platform

        if platform not in self._clang_tidy_config['platforms']:
            self.log(
                logging.ERROR, 'static-analysis', {},
                "ERROR: RUNNING: clang-tidy autotest for platform {} not supported.".format(
                    platform)
                )
            return self.TOOLS_UNSUPORTED_PLATFORM

        max_workers = multiprocessing.cpu_count()

        self.log(logging.INFO, 'static-analysis', {},
                 "RUNNING: clang-tidy autotest for platform {0} with {1} workers.".format(
                     platform, max_workers))

        # List all available checkers
        cmd = [self._clang_tidy_path, '-list-checks', '-checks=*']
        clang_output = subprocess.check_output(
            cmd, stderr=subprocess.STDOUT).decode('utf-8')
        available_checks = clang_output.split('\n')[1:]
        self._clang_tidy_checks = [c.strip() for c in available_checks if c]

        # Build the dummy compile_commands.json
        self._compilation_commands_path = self._create_temp_compilation_db(self._clang_tidy_config)
        checkers_test_batch = []
        checkers_results = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for item in self._clang_tidy_config['clang_checkers']:
                # Skip if any of the following statements is true:
                # 1. Checker attribute 'publish' is False.
                not_published = not bool(item.get('publish', True))
                # 2. Checker has restricted-platforms and current platform is not of them.
                ignored_platform = ('restricted-platforms' in item and
                                    platform not in item['restricted-platforms'])
                # 3. Checker name is mozilla-* or -*.
                ignored_checker = item['name'] in ['mozilla-*', '-*']
                # 4. List checker_names is passed and the current checker is not part of the
                #    list or 'publish' is False
                checker_not_in_list = checker_names and (
                    item['name'] not in checker_names or not_published)
                if not_published or \
                   ignored_platform or \
                   ignored_checker or \
                   checker_not_in_list:
                    continue
                checkers_test_batch.append(item['name'])
                futures.append(executor.submit(self._verify_checker, item, checkers_results))

            error_code = self.TOOLS_SUCCESS
            for future in concurrent.futures.as_completed(futures):
                # Wait for every task to finish
                ret_val = future.result()
                if ret_val != self.TOOLS_SUCCESS:
                    # We are interested only in one error and we don't break
                    # the execution of for loop since we want to make sure that all
                    # tasks finished.
                    error_code = ret_val

            if error_code != self.TOOLS_SUCCESS:

                self.log(logging.INFO, 'static-analysis', {},
                         "FAIL: the following clang-tidy check(s) failed:")
                for failure in checkers_results:
                    checker_error = failure['checker-error']
                    checker_name = failure['checker-name']
                    info1 = failure['info1']
                    info2 = failure['info2']
                    info3 = failure['info3']

                    message_to_log = ''
                    if checker_error == self.TOOLS_CHECKER_NOT_FOUND:
                        message_to_log = \
                            "\tChecker {} not present in this clang-tidy version.".format(
                                checker_name)
                    elif checker_error == self.TOOLS_CHECKER_NO_TEST_FILE:
                        message_to_log = \
                            "\tChecker {0} does not have a test file - {0}.cpp".format(
                                checker_name)
                    elif checker_error == self.TOOLS_CHECKER_RETURNED_NO_ISSUES:
                        message_to_log = (
                            "\tChecker {0} did not find any issues in its test file, "
                            "clang-tidy output for the run is:\n{1}"
                            ).format(checker_name, info1)
                    elif checker_error == self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND:
                        message_to_log = \
                            "\tChecker {0} does not have a result file - {0}.json".format(
                                checker_name)
                    elif checker_error == self.TOOLS_CHECKER_DIFF_FAILED:
                        message_to_log = (
                            "\tChecker {0}\nExpected: {1}\n"
                            "Got: {2}\n"
                            "clang-tidy output for the run is:\n"
                            "{3}"
                            ).format(checker_name, info1, info2, info3)

                    print('\n'+message_to_log)

                # Also delete the tmp folder
                shutil.rmtree(self._compilation_commands_path)
                return error_code

            # Run the analysis on all checkers at the same time only if we don't dump results.
            if not self._dump_results:
                ret_val = self._run_analysis_batch(checkers_test_batch)
                if ret_val != self.TOOLS_SUCCESS:
                    shutil.rmtree(self._compilation_commands_path)
                    return ret_val

        self.log(logging.INFO, 'static-analysis', {}, "SUCCESS: clang-tidy all tests passed.")
        # Also delete the tmp folder
        shutil.rmtree(self._compilation_commands_path)
        return self._autotest_infer(intree_tool, force_download, verbose)

    def _run_analysis(self, checks, header_filter, sources, jobs=1, fix=False, print_out=False):
        cmd = self._get_clang_tidy_command(
            checks=checks, header_filter=header_filter,
            sources=sources,
            jobs=jobs, fix=fix)

        try:
            clang_output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')
        except subprocess.CalledProcessError as e:
            print(e.output)
            return None
        return self._parse_issues(clang_output), clang_output

    def _run_analysis_batch(self, items):
        self.log(logging.INFO, 'static-analysis', {},
                 "RUNNING: clang-tidy checker batch analysis.")
        if not len(items):
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: clang-tidy checker list is empty!")
            return self.TOOLS_CHECKER_LIST_EMPTY

        issues, clang_output = self._run_analysis(
            checks='-*,' + ",".join(items),
            header_filter='',
            sources=[mozpath.join(self._clang_tidy_base_path, "test", checker) + '.cpp'
                     for checker in items],
            print_out=True)

        if issues is None:
            return self.TOOLS_CHECKER_FAILED_FILE

        failed_checks = []
        failed_checks_baseline = []
        for checker in items:
            test_file_path_json = mozpath.join(
                self._clang_tidy_base_path, "test", checker) + '.json'
            # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)

            # We also stored the 'reliability' index so strip that from the baseline_issues
            baseline_issues[:] = [item for item in baseline_issues if 'reliability' not in item]

            found = all([element_base in issues for element_base in baseline_issues])

            if not found:
                failed_checks.append(checker)
                failed_checks_baseline.append(baseline_issues)

        if len(failed_checks) > 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: The following check(s) failed for bulk analysis: ' +
                     ' '.join(failed_checks))

            for failed_check, baseline_issue in zip(failed_checks, failed_checks_baseline):
                print('\tChecker {0} expect following results: \n\t\t{1}'.format(
                    failed_check, baseline_issue))

            print('This is the output generated by clang-tidy for the bulk build:\n{}'.format(
                clang_output))
            return self.TOOLS_CHECKER_DIFF_FAILED

        return self.TOOLS_SUCCESS

    def _create_temp_compilation_db(self, config):
        directory = tempfile.mkdtemp(prefix='cc')
        with open(mozpath.join(directory, "compile_commands.json"), "wb") as file_handler:
            compile_commands = []
            director = mozpath.join(self.topsrcdir, 'tools', 'clang-tidy', 'test')
            for item in config['clang_checkers']:
                if item['name'] in ['-*', 'mozilla-*']:
                    continue
                file = item['name'] + '.cpp'
                element = {}
                element["directory"] = director
                element["command"] = 'cpp ' + file
                element["file"] = mozpath.join(director, file)
                compile_commands.append(element)

            json.dump(compile_commands, file_handler)
            file_handler.flush()

            return directory

    def _autotest_infer(self, intree_tool, force_download, verbose):
        # infer is not available on other platforms, but autotest should work even without
        # it being installed
        if self.platform[0] == 'linux64':
            rc = self._check_for_java()
            if rc != 0:
                return 1
            rc = self._get_infer(force=force_download, verbose=verbose, intree_tool=intree_tool)
            if rc != 0:
                self.log(logging.ERROR, 'ERROR: static-analysis', {},
                         'ERROR: infer unable to locate package.')
                return self.TOOLS_FAILED_DOWNLOAD
            self.__infer_tool = mozpath.join(self.topsrcdir, 'tools', 'infer')
            self.__infer_test_folder = mozpath.join(self.__infer_tool, 'test')

            max_workers = multiprocessing.cpu_count()
            self.log(logging.INFO, 'static-analysis', {},
                     "RUNNING: infer autotest for platform {0} with {1} workers.".format(
                         self.platform[0], max_workers))
            # clean previous autotest if it exists
            rc = self._gradle(['autotest:clean'], autotest=True)
            if rc != 0:
                return rc
            import yaml
            with open(mozpath.join(self.__infer_tool, 'config.yaml')) as f:
                config = yaml.safe_load(f)
            with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
                futures = []
                for item in config['infer_checkers']:
                    if item['publish']:
                        futures.append(executor.submit(self._verify_infer_checker, item))
                # this is always included in check-java, but not in config.yaml
                futures.append(executor.submit(self._verify_infer_checker,
                                               {'name': 'checkers'}))
                for future in concurrent.futures.as_completed(futures):
                    ret_val = future.result()
                    if ret_val != self.TOOLS_SUCCESS:
                        return ret_val
            self.log(logging.INFO, 'static-analysis', {}, "SUCCESS: infer all tests passed.")
        else:
            self.log(logging.WARNING, 'static-analysis', {},
                     "Skipping infer autotest, because it is only available on linux64!")
        return self.TOOLS_SUCCESS

    def _verify_infer_checker(self, item):
        '''Given a checker, this method verifies the following:
          1. if there is a `checker`.json and `checker`.java file in
             `tools/infer/test/autotest/src`
          2. if running infer on `checker`.java yields the same result as `checker`.json
        An `item` is simply a dictionary, which needs to have a `name` field set, which is the
        name of the checker.
        '''
        def to_camelcase(str):
            return ''.join([s.capitalize() for s in str.split('-')])
        check = item['name']
        test_file_path = mozpath.join(self.__infer_tool, 'test', 'autotest', 'src',
                                      'main', 'java', to_camelcase(check))
        test_file_path_java = test_file_path + '.java'
        test_file_path_json = test_file_path + '.json'
        self.log(logging.INFO, 'static-analysis', {}, "RUNNING: infer check {}.".format(check))
        # Verify if the test file exists for this checker
        if not os.path.exists(test_file_path_java):
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: infer check {} doesn't have a test file.".format(check))
            return self.TOOLS_CHECKER_NO_TEST_FILE
        # run infer on a particular test file
        out_folder = mozpath.join(self.__infer_test_folder, 'test-infer-{}'.format(check))
        if check == 'checkers':
            check_arg = ['-a', 'checkers']
        else:
            check_arg = ['--{}-only'.format(check)]
        infer_args = [self._infer_path, 'run'] + check_arg + ['-o', out_folder, '--']
        gradle_args = ['autotest:compileInferTest{}'.format(to_camelcase(check))]
        rc = self._gradle(gradle_args, infer_args=infer_args, autotest=True)
        if rc != 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: infer failed to execute gradle {}.".format(gradle_args))
            return self.TOOLS_GRADLE_FAILED
        issues = json.load(open(mozpath.join(out_folder, 'report.json')))
        # remove folder that infer creates because the issues are loaded into memory
        shutil.rmtree(out_folder)
        # Verify to see if we got any issues, if not raise exception
        if not issues:
            self.log(
                logging.ERROR, 'static-analysis', {},
                "ERROR: infer check {0} did not find any issues in its associated test suite."
                .format(check)
            )
            return self.TOOLS_CHECKER_RETURNED_NO_ISSUES
        if self._dump_results:
            self._build_autotest_result(test_file_path_json, json.dumps(issues))
        else:
            if not os.path.exists(test_file_path_json):
                # Result file for test not found maybe regenerate it?
                self.log(
                    logging.ERROR, 'static-analysis', {},
                    "ERROR: infer result file not found for check {0}".format(check)
                )
                return self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND
            # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)

            def ordered(obj):
                if isinstance(obj, dict):
                    return sorted((k, ordered(v)) for k, v in obj.items())
                if isinstance(obj, list):
                    return sorted(ordered(x) for x in obj)
                return obj
            # Compare the two lists
            if ordered(issues) != ordered(baseline_issues):
                error_str = "ERROR: in check {} Expected: ".format(check)
                error_str += '\n' + json.dumps(baseline_issues, indent=2)
                error_str += '\n Got:\n' + json.dumps(issues, indent=2)
                self.log(logging.ERROR, 'static-analysis', {},
                         'ERROR: infer autotest for check {} failed, check stdout for more details'
                         .format(check))
                print(error_str)
                return self.TOOLS_CHECKER_DIFF_FAILED
        return self.TOOLS_SUCCESS

    @StaticAnalysisSubCommand('static-analysis', 'install',
                              'Install the static analysis helper tool')
    @CommandArgument('source', nargs='?', type=str,
                     help='Where to fetch a local archive containing the static-analysis and '
                     'format helper tool.'
                          'It will be installed in ~/.mozbuild/clang-tools and ~/.mozbuild/infer.'
                          'Can be omitted, in which case the latest clang-tools and infer '
                          'helper for the platform would be automatically detected and installed.')
    @CommandArgument('--skip-cache', action='store_true',
                     help='Skip all local caches to force re-fetching the helper tool.',
                     default=False)
    @CommandArgument('--force', action='store_true',
                     help='Force re-install even though the tool exists in mozbuild.',
                     default=False)
    @CommandArgument('--minimal-install', action='store_true',
                     help='Download only clang based tool.',
                     default=False)
    def install(self, source=None, skip_cache=False, force=False, minimal_install=False,
                verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(force=force, skip_cache=skip_cache,
                                   source=source, verbose=verbose)
        if rc == 0 and not minimal_install:
            # XXX ignore the return code because if it fails or not, infer is
            # not mandatory, but clang-tidy is
            self._get_infer(force=force, skip_cache=skip_cache, verbose=verbose)
        return rc

    @StaticAnalysisSubCommand('static-analysis', 'clear-cache',
                              'Delete local helpers and reset static analysis helper tool cache')
    def clear_cache(self, verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(force=True, download_if_needed=True, skip_cache=True,
                                   verbose=verbose)
        if rc == 0:
            self._get_infer(force=True, download_if_needed=True, skip_cache=True,
                            verbose=verbose)
        if rc != 0:
            return rc
        return self._artifact_manager.artifact_clear_cache()

    @StaticAnalysisSubCommand('static-analysis', 'print-checks',
                              'Print a list of the static analysis checks performed by default')
    def print_checks(self, verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(verbose=verbose)
        if rc == 0:
            rc = self._get_infer(verbose=verbose)
        if rc != 0:
            return rc
        args = [self._clang_tidy_path, '-list-checks', '-checks=%s' % self._get_checks()]
        rc = self._run_command_in_objdir(args=args, pass_thru=True)
        if rc != 0:
            return rc
        checkers, _ = self._get_infer_config()
        print('Infer checks:')
        for checker in checkers:
            print(' '*4 + checker)
        return 0

    @Command('prettier-format',  category='misc', description='Run prettier on current changes')
    @CommandArgument('--path', '-p', nargs=1, required=True,
                     help='Specify the path to reformat to stdout.')
    @CommandArgument('--assume-filename', '-a', nargs=1, required=True,
                     help='This option is usually used in the context of hg-formatsource.'
                          'When reading from stdin, Prettier assumes this '
                          'filename to decide which style and parser to use.')
    def prettier_format(self, path, assume_filename):
        # With assume_filename we want to have stdout clean since the result of the
        # format will be redirected to stdout.

        binary, _ = find_node_executable()
        prettier = os.path.join(self.topsrcdir, "node_modules", "prettier", "bin-prettier.js")
        path = os.path.join(self.topsrcdir, path[0])

        # Bug 1564824. Prettier fails on patches with moved files where the
        # original directory also does not exist.
        assume_dir = os.path.dirname(os.path.join(self.topsrcdir, assume_filename[0]))
        assume_filename = assume_filename[0] if os.path.isdir(assume_dir) else path

        # We use --stdin-filepath in order to better determine the path for
        # the prettier formatter when it is ran outside of the repo, for example
        # by the extension hg-formatsource.
        args = [binary, prettier, '--stdin-filepath', assume_filename]

        process = subprocess.Popen(args, stdin=subprocess.PIPE)
        with open(path, 'r') as fin:
            process.stdin.write(fin.read())
            process.stdin.close()
            process.wait()
            return process.returncode

    @StaticAnalysisSubCommand('static-analysis', 'check-syntax',
                              'Run the check-syntax for C/C++ files based on '
                              '`compile_commands.json`')
    @CommandArgument('source', nargs='*',
                     help='Source files to be compiled checked (regex on path).')
    def check_syntax(self, source, verbose=False):
        self._set_log_level(verbose)
        self._activate_virtualenv()
        self.log_manager.enable_unstructured()

        # Verify that we have a valid `source`
        if len(source) == 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Specify files that need to be syntax checked.')
            return

        rc = self._build_compile_db(verbose=verbose)
        if rc != 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Unable to build the `compile_commands.json`.')
            return rc
        rc = self._build_export(jobs=2, verbose=verbose)

        if rc != 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Unable to build export.')
            return rc

        # Build the list with all files from source
        path_list = self._generate_path_list(source)

        compile_db = json.load(open(self._compile_db, 'r'))

        if compile_db is None:
            self.log(logging.ERROR, 'static-analysis', {},
                     'ERROR: Loading {}'.format(self._compile_db))
            return 1

        commands = []

        compile_dict = {entry['file']: entry['command']
                        for entry in compile_db}
        # Begin the compile check for each file
        for file in path_list:
            # It must be a C/C++ file
            ext = mozpath.splitext(file)[-1]

            if ext.lower() not in self._check_syntax_include_extensions:
                self.log(logging.INFO, 'static-analysis',
                         {}, 'Skipping {}'.format(file))
                continue
            file_with_abspath = mozpath.join(self.topsrcdir, file)
            # Found for a file that we are looking

            entry = compile_dict.get(file_with_abspath, None)
            if entry:
                command = entry.split(' ')
                # Verify to see if we are dealing with an unified build
                if 'Unified_' in command[-1]:
                    # Translate the unified `TU` to per file basis TU
                    command[-1] = file_with_abspath

                # We want syntax-only
                command.append('-fsyntax-only')
                commands.append(command)

        max_workers = multiprocessing.cpu_count()

        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for command in commands:
                futures.append(executor.submit(self.run_process, args=command,
                                               cwd=self.topsrcdir, pass_thru=True,
                                               ensure_exit_code=False))

    @Command('clang-format',  category='misc', description='Run clang-format on current changes')
    @CommandArgument('--show', '-s', action='store_const', const='stdout', dest='output_path',
                     help='Show diff output on stdout instead of applying changes')
    @CommandArgument('--assume-filename', '-a', nargs=1, default=None,
                     help='This option is usually used in the context of hg-formatsource.'
                          'When reading from stdin, clang-format assumes this '
                          'filename to look for a style config file (with '
                          '-style=file) and to determine the language. When '
                          'specifying this option only one file should be used '
                          'as an input and the output will be forwarded to stdin. '
                          'This option also impairs the download of the clang-tools '
                          'and assumes the package is already located in it\'s default '
                          'location')
    @CommandArgument('--path', '-p', nargs='+', default=None,
                     help='Specify the path(s) to reformat')
    @CommandArgument('--commit', '-c', default=None,
                     help='Specify a commit to reformat from.'
                          'For git you can also pass a range of commits (foo..bar)'
                          'to format all of them at the same time.')
    @CommandArgument('--output', '-o', default=None, dest='output_path',
                     help='Specify a file handle to write clang-format raw output instead of '
                          'applying changes. This can be stdout or a file path.')
    @CommandArgument('--format', '-f', choices=('diff', 'json'), default='diff',
                     dest='output_format',
                     help='Specify the output format used: diff is the raw patch provided by '
                     'clang-format, json is a list of atomic changes to process.')
    @CommandArgument('--outgoing', default=False, action='store_true',
                     help='Run clang-format on outgoing files from mercurial repository')
    def clang_format(self, assume_filename, path, commit, output_path=None, output_format='diff',
                     verbose=False, outgoing=False):
        # Run clang-format or clang-format-diff on the local changes
        # or files/directories
        if path is None and outgoing:
            repo = get_repository_object(self.topsrcdir)
            path = repo.get_outgoing_files()

        if path:
            # Create the full path list
            def path_maker(f_name): return os.path.join(self.topsrcdir, f_name)
            path = map(path_maker, path)

        os.chdir(self.topsrcdir)

        # Load output file handle, either stdout or a file handle in write mode
        output = None
        if output_path is not None:
            output = sys.stdout if output_path == 'stdout' else open(output_path, 'w')

        # With assume_filename we want to have stdout clean since the result of the
        # format will be redirected to stdout. Only in case of errror we
        # write something to stdout.
        # We don't actually want to get the clang-tools here since we want in some
        # scenarios to do this in parallel so we relay on the fact that the tools
        # have already been downloaded via './mach bootstrap' or directly via
        # './mach static-analysis install'
        if assume_filename:
            rc = self._set_clang_tools_paths()
            if rc != 0:
                print("clang-format: Unable to set path to clang-format tools.")
                return rc

            if not self._do_clang_tools_exist():
                print("clang-format: Unable to set locate clang-format tools.")
                return 1
        else:
            rc = self._get_clang_tools(verbose=verbose)
            if rc != 0:
                return rc

        if self._is_version_eligible() is False:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: You're using an old version of clang-format binary."
                     " Please update to a more recent one by running: './mach bootstrap'")
            return 1

        if path is None:
            return self._run_clang_format_diff(self._clang_format_diff,
                                               self._clang_format_path, commit, output)

        if assume_filename:
            return self._run_clang_format_in_console(self._clang_format_path,
                                                     path, assume_filename)

        return self._run_clang_format_path(self._clang_format_path, path, output, output_format)

    def _verify_checker(self, item, checkers_results):
        check = item['name']
        test_file_path = mozpath.join(self._clang_tidy_base_path, "test", check)
        test_file_path_cpp = test_file_path + '.cpp'
        test_file_path_json = test_file_path + '.json'

        self.log(logging.INFO, 'static-analysis', {},
                 "RUNNING: clang-tidy checker {}.".format(check))

        # Structured information in case a checker fails
        checker_error = {
            'checker-name': check,
            'checker-error': '',
            'info1': '',
            'info2': '',
            'info3': ''
        }

        # Verify if this checker actually exists
        if check not in self._clang_tidy_checks:
            checker_error['checker-error'] = self.TOOLS_CHECKER_NOT_FOUND
            checkers_results.append(checker_error)
            return self.TOOLS_CHECKER_NOT_FOUND

        # Verify if the test file exists for this checker
        if not os.path.exists(test_file_path_cpp):
            checker_error['checker-error'] = self.TOOLS_CHECKER_NO_TEST_FILE
            checkers_results.append(checker_error)
            return self.TOOLS_CHECKER_NO_TEST_FILE

        issues, clang_output = self._run_analysis(
            checks='-*,' + check, header_filter='', sources=[test_file_path_cpp])
        if issues is None:
            return self.TOOLS_CHECKER_FAILED_FILE

        # Verify to see if we got any issues, if not raise exception
        if not issues:
            checker_error['checker-error'] = self.TOOLS_CHECKER_RETURNED_NO_ISSUES
            checker_error['info1'] = clang_output
            checkers_results.append(checker_error)
            return self.TOOLS_CHECKER_RETURNED_NO_ISSUES

        # Also store the 'reliability' index for this checker
        issues.append({'reliability': item['reliability']})

        if self._dump_results:
            self._build_autotest_result(test_file_path_json, json.dumps(issues))
        else:
            if not os.path.exists(test_file_path_json):
                # Result file for test not found maybe regenerate it?
                checker_error['checker-error'] = self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND
                checkers_results.append(checker_error)
                return self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND

            # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)

            # Compare the two lists
            if issues != baseline_issues:
                checker_error['checker-error'] = self.TOOLS_CHECKER_DIFF_FAILED
                checker_error['info1'] = baseline_issues
                checker_error['info2'] = issues
                checker_error['info3'] = clang_output
                checkers_results.append(checker_error)
                return self.TOOLS_CHECKER_DIFF_FAILED

        return self.TOOLS_SUCCESS

    def _build_autotest_result(self, file, issues):
        with open(file, 'w') as f:
            f.write(issues)

    def _get_autotest_stored_issues(self, file):
        with open(file) as f:
            return json.load(f)

    def _parse_issues(self, clang_output):
        '''
        Parse clang-tidy output into structured issues
        '''

        # Limit clang output parsing to 'Enabled checks:'
        end = re.search(r'^Enabled checks:\n', clang_output, re.MULTILINE)
        if end is not None:
            clang_output = clang_output[:end.start()-1]

        platform, _ = self.platform
        # Starting with clang 8, for the diagnostic messages we have multiple `LF CR`
        # in order to be compatiable with msvc compiler format, and for this
        # we are not interested to match the end of line.
        regex_string = r'(.+):(\d+):(\d+): (warning|error): ([^\[\]\n]+)(?: \[([\.\w-]+)\])'

        # For non 'win' based platforms we also need the 'end of the line' regex
        if platform not in ('win64', 'win32'):
            regex_string += '?$'

        regex_header = re.compile(regex_string, re.MULTILINE)

        # Sort headers by positions
        headers = sorted(
            regex_header.finditer(clang_output),
            key=lambda h: h.start()
        )
        issues = []
        for _, header in enumerate(headers):
            header_group = header.groups()
            element = [header_group[3], header_group[4], header_group[5]]
            issues.append(element)
        return issues

    def _get_checks(self):
        checks = '-*'
        try:
            config = self._clang_tidy_config
            for item in config['clang_checkers']:
                if item.get('publish', True):
                    checks += ',' + item['name']
        except Exception:
            print('Looks like config.yaml is not valid, so we are unable to '
                  'determine default checkers, using \'-checks=-*,mozilla-*\'')
            checks += ',mozilla-*'
        finally:
            return checks

    def _get_checks_config(self):
        config_list = []
        checker_config = {}
        try:
            config = self._clang_tidy_config
            for checker in config['clang_checkers']:
                if checker.get('publish', True) and 'config' in checker:
                    for checker_option in checker['config']:
                        # Verify if the format of the Option is correct,
                        # possibilities are:
                        # 1. CheckerName.Option
                        # 2. Option -> that will become CheckerName.Option
                        if not checker_option['key'].startswith(checker['name']):
                            checker_option['key'] = "{}.{}".format(
                                checker['name'], checker_option['key'])
                    config_list += checker['config']
            checker_config['CheckOptions'] = config_list
        except Exception:
            print('Looks like config.yaml is not valid, so we are unable to '
                  'determine configuration for checkers, so using default')
            checker_config = None
        finally:
            return checker_config

    def _get_config_environment(self):
        ran_configure = False
        config = None
        builder = Build(self._mach_context)

        try:
            config = self.config_environment
        except Exception:
            self.log(logging.WARNING, 'static-analysis', {},
                     "Looks like configure has not run yet, running it now...")

            clobber = Clobberer(self.topsrcdir, self.topobjdir)

            if clobber.clobber_needed():
                choice = prompt_bool(
                    "Configuration has changed and Clobber is needed. "
                    "Do you want to proceed?"
                )
                if not choice:
                    self.log(logging.ERROR, 'static-analysis', {},
                             "ERROR: Without Clobber we cannot continue execution!")
                    return (1, None, None)
                os.environ["AUTOCLOBBER"] = "1"

            rc = builder.configure()
            if rc != 0:
                return (rc, config, ran_configure)
            ran_configure = True
            try:
                config = self.config_environment
            except Exception:
                pass

        return (0, config, ran_configure)

    def _build_compile_db(self, verbose=False):
        self._compile_db = mozpath.join(self.topobjdir, 'compile_commands.json')
        if os.path.exists(self._compile_db):
            return 0

        rc, config, ran_configure = self._get_config_environment()
        if rc != 0:
            return rc

        if ran_configure:
            # Configure may have created the compilation database if the
            # mozconfig enables building the CompileDB backend by default,
            # So we recurse to see if the file exists once again.
            return self._build_compile_db(verbose=verbose)

        if config:
            print('Looks like a clang compilation database has not been '
                  'created yet, creating it now...')
            builder = Build(self._mach_context)
            rc = builder.build_backend(['CompileDB'], verbose=verbose)
            if rc != 0:
                return rc
            assert os.path.exists(self._compile_db)
            return 0

    def _build_export(self, jobs, verbose=False):
        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        builder = Build(self._mach_context)
        # First install what we can through install manifests.
        rc = builder._run_make(directory=self.topobjdir, target='pre-export',
                               line_handler=None, silent=not verbose)
        if rc != 0:
            return rc

        # Then build the rest of the build dependencies by running the full
        # export target, because we can't do anything better.
        return builder._run_make(directory=self.topobjdir, target='export',
                                 line_handler=None, silent=not verbose,
                                 num_jobs=jobs)

    def _set_clang_tools_paths(self):
        rc, config, _ = self._get_config_environment()

        if rc != 0:
            return rc

        self._clang_tools_path = mozpath.join(self._mach_context.state_dir, "clang-tools")
        self._clang_tidy_path = mozpath.join(self._clang_tools_path, "clang-tidy", "bin",
                                             "clang-tidy" + config.substs.get('BIN_SUFFIX', ''))
        self._clang_format_path = mozpath.join(
            self._clang_tools_path, "clang-tidy", "bin",
            "clang-format" + config.substs.get('BIN_SUFFIX', ''))
        self._clang_apply_replacements = mozpath.join(
            self._clang_tools_path, "clang-tidy", "bin",
            "clang-apply-replacements" + config.substs.get('BIN_SUFFIX', ''))
        self._run_clang_tidy_path = mozpath.join(self._clang_tools_path, "clang-tidy",
                                                 "share", "clang", "run-clang-tidy.py")
        self._clang_format_diff = mozpath.join(self._clang_tools_path, "clang-tidy",
                                               "share", "clang", "clang-format-diff.py")
        return 0

    def _do_clang_tools_exist(self):
        return os.path.exists(self._clang_tidy_path) and \
               os.path.exists(self._clang_format_path) and \
               os.path.exists(self._clang_apply_replacements) and \
               os.path.exists(self._run_clang_tidy_path)

    def _get_clang_tools(self, force=False, skip_cache=False,
                         source=None, download_if_needed=True,
                         verbose=False):

        rc = self._set_clang_tools_paths()

        if rc != 0:
            return rc

        if self._do_clang_tools_exist() and not force:
            return 0

        if os.path.isdir(self._clang_tools_path) and download_if_needed:
            # The directory exists, perhaps it's corrupted?  Delete it
            # and start from scratch.
            shutil.rmtree(self._clang_tools_path)
            return self._get_clang_tools(force=force, skip_cache=skip_cache,
                                         source=source, verbose=verbose,
                                         download_if_needed=download_if_needed)

        # Create base directory where we store clang binary
        os.mkdir(self._clang_tools_path)

        if source:
            return self._get_clang_tools_from_source(source)

        from mozbuild.artifact_commands import PackageFrontend

        self._artifact_manager = PackageFrontend(self._mach_context)

        if not download_if_needed:
            return 0

        job, _ = self.platform

        if job is None:
            raise Exception('The current platform isn\'t supported. '
                            'Currently only the following platforms are '
                            'supported: win32/win64, linux64 and macosx64.')

        job += '-clang-tidy'

        # We want to unpack data in the clang-tidy mozbuild folder
        currentWorkingDir = os.getcwd()
        os.chdir(self._clang_tools_path)
        rc = self._artifact_manager.artifact_toolchain(verbose=verbose,
                                                       skip_cache=skip_cache,
                                                       from_build=[job],
                                                       no_unpack=False,
                                                       retry=0)
        # Change back the cwd
        os.chdir(currentWorkingDir)

        return rc

    def _get_clang_tools_from_source(self, filename):
        from mozbuild.action.tooltool import unpack_file
        clang_tidy_path = mozpath.join(self._mach_context.state_dir,
                                       "clang-tools")

        currentWorkingDir = os.getcwd()
        os.chdir(clang_tidy_path)

        unpack_file(filename)

        # Change back the cwd
        os.chdir(currentWorkingDir)

        clang_path = mozpath.join(clang_tidy_path, 'clang')

        if not os.path.isdir(clang_path):
            raise Exception('Extracted the archive but didn\'t find '
                            'the expected output')

        assert os.path.exists(self._clang_tidy_path)
        assert os.path.exists(self._clang_format_path)
        assert os.path.exists(self._clang_apply_replacements)
        assert os.path.exists(self._run_clang_tidy_path)
        return 0

    def _get_clang_format_diff_command(self, commit):
        if self.repository.name == 'hg':
            args = ["hg", "diff", "-U0"]
            if commit:
                args += ["-c", commit]
            else:
                args += ["-r", ".^"]
            for dot_extension in self._format_include_extensions:
                args += ['--include', 'glob:**{0}'.format(dot_extension)]
            args += ['--exclude', 'listfile:{0}'.format(self._format_ignore_file)]
        else:
            commit_range = "HEAD"  # All uncommitted changes.
            if commit:
                commit_range = commit if ".." in commit else "{}~..{}".format(commit, commit)
            args = ["git", "diff", "--no-color", "-U0", commit_range, "--"]
            for dot_extension in self._format_include_extensions:
                args += ['*{0}'.format(dot_extension)]
            # git-diff doesn't support an 'exclude-from-files' param, but
            # allow to add individual exclude pattern since v1.9, see
            # https://git-scm.com/docs/gitglossary#gitglossary-aiddefpathspecapathspec
            with open(self._format_ignore_file, 'rb') as exclude_pattern_file:
                for pattern in exclude_pattern_file.readlines():
                    pattern = pattern.rstrip()
                    pattern = pattern.replace('.*', '**')
                    if not pattern or pattern.startswith('#'):
                        continue  # empty or comment
                    magics = ['exclude']
                    if pattern.startswith('^'):
                        magics += ['top']
                        pattern = pattern[1:]
                    args += [':({0}){1}'.format(','.join(magics), pattern)]
        return args

    def _get_infer(self, force=False, skip_cache=False, download_if_needed=True,
                   verbose=False, intree_tool=False):
        rc, config, _ = self._get_config_environment()
        if rc != 0:
            return rc
        infer_path = os.environ['MOZ_FETCHES_DIR'] if intree_tool else \
            mozpath.join(self._mach_context.state_dir, 'infer')
        self._infer_path = mozpath.join(infer_path, 'infer', 'bin', 'infer' +
                                        config.substs.get('BIN_SUFFIX', ''))
        if intree_tool:
            return not os.path.exists(self._infer_path)
        if os.path.exists(self._infer_path) and not force:
            return 0

        if os.path.isdir(infer_path) and download_if_needed:
            # The directory exists, perhaps it's corrupted?  Delete it
            # and start from scratch.
            shutil.rmtree(infer_path)
            return self._get_infer(force=force, skip_cache=skip_cache,
                                   verbose=verbose,
                                   download_if_needed=download_if_needed)
        os.mkdir(infer_path)
        from mozbuild.artifact_commands import PackageFrontend
        self._artifact_manager = PackageFrontend(self._mach_context)
        if not download_if_needed:
            return 0
        job, _ = self.platform
        if job != 'linux64':
            return -1
        else:
            job += '-infer'
        # We want to unpack data in the infer mozbuild folder
        currentWorkingDir = os.getcwd()
        os.chdir(infer_path)
        rc = self._artifact_manager.artifact_toolchain(verbose=verbose,
                                                       skip_cache=skip_cache,
                                                       from_build=[job],
                                                       no_unpack=False,
                                                       retry=0)
        # Change back the cwd
        os.chdir(currentWorkingDir)
        return rc

    def _run_clang_format_diff(self, clang_format_diff, clang_format, commit, output_file):
        # Run clang-format on the diff
        # Note that this will potentially miss a lot things
        from subprocess import Popen, PIPE, check_output, CalledProcessError

        diff_process = Popen(self._get_clang_format_diff_command(commit), stdout=PIPE)
        args = [sys.executable, clang_format_diff, "-p1", "-binary=%s" % clang_format]

        if not output_file:
            args.append("-i")
        try:
            output = check_output(args, stdin=diff_process.stdout)
            if output_file:
                # We want to print the diffs
                print(output, file=output_file)

            return 0
        except CalledProcessError as e:
            # Something wrong happend
            print("clang-format: An error occured while running clang-format-diff.")
            return e.returncode

    def _is_ignored_path(self, ignored_dir_re, f):
        # Remove upto topsrcdir in pathname and match
        if f.startswith(self.topsrcdir + '/'):
            match_f = f[len(self.topsrcdir + '/'):]
        else:
            match_f = f
        return re.match(ignored_dir_re, match_f)

    def _generate_path_list(self, paths, verbose=True):
        path_to_third_party = os.path.join(self.topsrcdir, self._format_ignore_file)
        ignored_dir = []
        with open(path_to_third_party, 'r') as fh:
            for line in fh:
                # Remove comments and empty lines
                if line.startswith('#') or len(line.strip()) == 0:
                    continue
                # The regexp is to make sure we are managing relative paths
                ignored_dir.append(r"^[\./]*" + line.rstrip())

        # Generates the list of regexp
        ignored_dir_re = '(%s)' % '|'.join(ignored_dir)
        extensions = self._format_include_extensions

        path_list = []
        for f in paths:
            if self._is_ignored_path(ignored_dir_re, f):
                # Early exit if we have provided an ignored directory
                if verbose:
                    print("static-analysis: Ignored third party code '{0}'".format(f))
                continue

            if os.path.isdir(f):
                # Processing a directory, generate the file list
                for folder, subs, files in os.walk(f):
                    subs.sort()
                    for filename in sorted(files):
                        f_in_dir = os.path.join(folder, filename)
                        if (f_in_dir.endswith(extensions)
                            and not self._is_ignored_path(ignored_dir_re, f_in_dir)):
                            # Supported extension and accepted path
                            path_list.append(f_in_dir)
            else:
                # Make sure that the file exists and it has a supported extension
                if os.path.isfile(f) and f.endswith(extensions):
                    path_list.append(f)

        return path_list

    def _run_clang_format_in_console(self, clang_format, paths, assume_filename):
        path_list = self._generate_path_list(assume_filename, False)

        if path_list == []:
            return 0

        # We use -assume-filename in order to better determine the path for
        # the .clang-format when it is ran outside of the repo, for example
        # by the extension hg-formatsource
        args = [clang_format, "-assume-filename={}".format(assume_filename[0])]

        process = subprocess.Popen(args, stdin=subprocess.PIPE)
        with open(paths[0], 'r') as fin:
            process.stdin.write(fin.read())
            process.stdin.close()
            process.wait()
            return process.returncode

    def _get_clang_format_cfg(self, current_dir):
        clang_format_cfg_path = mozpath.join(current_dir, '.clang-format')

        if os.path.exists(clang_format_cfg_path):
            # Return found path for .clang-format
            return clang_format_cfg_path

        if current_dir != self.topsrcdir:
            # Go to parent directory
            return self._get_clang_format_cfg(os.path.split(current_dir)[0])
        # We have reached self.topsrcdir so return None
        return None

    def _copy_clang_format_for_show_diff(self, current_dir, cached_clang_format_cfg, tmpdir):
        # Lookup for .clang-format first in cache
        clang_format_cfg = cached_clang_format_cfg.get(current_dir, None)

        if clang_format_cfg is None:
            # Go through top directories
            clang_format_cfg = self._get_clang_format_cfg(current_dir)

            # This is unlikely to happen since we must have .clang-format from
            # self.topsrcdir but in any case we should handle a potential error
            if clang_format_cfg is None:
                print("Cannot find corresponding .clang-format.")
                return 1

            # Cache clang_format_cfg for potential later usage
            cached_clang_format_cfg[current_dir] = clang_format_cfg

        # Copy .clang-format to the tmp dir where the formatted file is copied
        shutil.copy(clang_format_cfg, tmpdir)
        return 0

    def _run_clang_format_path(self, clang_format, paths, output_file, output_format):

        # Run clang-format on files or directories directly
        from subprocess import check_output, CalledProcessError

        if output_format == 'json':
            # Get replacements in xml, then process to json
            args = [clang_format, '-output-replacements-xml']
        else:
            args = [clang_format, '-i']

        if output_file:
            # We just want to show the diff, we create the directory to copy it
            tmpdir = os.path.join(self.topobjdir, 'tmp')
            if not os.path.exists(tmpdir):
                os.makedirs(tmpdir)

        path_list = self._generate_path_list(paths)

        if path_list == []:
            return

        print("Processing %d file(s)..." % len(path_list))

        if output_file:
            patches = {}
            cached_clang_format_cfg = {}
            for i in range(0, len(path_list)):
                l = path_list[i: (i + 1)]

                # Copy the files into a temp directory
                # and run clang-format on the temp directory
                # and show the diff
                original_path = l[0]
                local_path = ntpath.basename(original_path)
                current_dir = ntpath.dirname(original_path)
                target_file = os.path.join(tmpdir, local_path)
                faketmpdir = os.path.dirname(target_file)
                if not os.path.isdir(faketmpdir):
                    os.makedirs(faketmpdir)
                shutil.copy(l[0], faketmpdir)
                l[0] = target_file

                ret = self._copy_clang_format_for_show_diff(current_dir,
                                                            cached_clang_format_cfg,
                                                            faketmpdir)
                if ret != 0:
                    return ret

                # Run clang-format on the list
                try:
                    output = check_output(args + l)
                    if output and output_format == 'json':
                        # Output a relative path in json patch list
                        relative_path = os.path.relpath(original_path, self.topsrcdir)
                        patches[relative_path] = self._parse_xml_output(original_path, output)
                except CalledProcessError as e:
                    # Something wrong happend
                    print("clang-format: An error occured while running clang-format.")
                    return e.returncode

                # show the diff
                if output_format == 'diff':
                    diff_command = ["diff", "-u", original_path, target_file]
                    try:
                        output = check_output(diff_command)
                    except CalledProcessError as e:
                        # diff -u returns 0 when no change
                        # here, we expect changes. if we are here, this means that
                        # there is a diff to show
                        if e.output:
                            # Replace the temp path by the path relative to the repository to
                            # display a valid patch
                            relative_path = os.path.relpath(original_path, self.topsrcdir)
                            patch = e.output.replace(target_file, relative_path)
                            patch = patch.replace(original_path, relative_path)
                            patches[original_path] = patch

            if output_format == 'json':
                output = json.dumps(patches, indent=4)
            else:
                # Display all the patches at once
                output = '\n'.join(patches.values())

            # Output to specified file or stdout
            print(output, file=output_file)

            shutil.rmtree(tmpdir)
            return 0

        # Run clang-format in parallel trying to saturate all of the available cores.
        import math

        max_workers = multiprocessing.cpu_count()

        # To maximize CPU usage when there are few items to handle,
        # underestimate the number of items per batch, then dispatch
        # outstanding items across workers. Per definition, each worker will
        # handle at most one outstanding item.
        batch_size = int(math.floor(float(len(path_list)) / max_workers))
        outstanding_items = len(path_list) - batch_size * max_workers

        batches = []

        i = 0
        while i < len(path_list):
            num_items = batch_size + (1 if outstanding_items > 0 else 0)
            batches.append(args + path_list[i: (i + num_items)])

            outstanding_items -= 1
            i += num_items

        error_code = None

        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for batch in batches:
                futures.append(executor.submit(run_one_clang_format_batch, batch))

            for future in concurrent.futures.as_completed(futures):
                # Wait for every task to finish
                ret_val = future.result()
                if ret_val is not None:
                    error_code = ret_val

            if error_code is not None:
                return error_code
        return 0

    def _parse_xml_output(self, path, clang_output):
        '''
        Parse the clang-format XML output to convert it in a JSON compatible
        list of patches, and calculates line level informations from the
        character level provided changes.
        '''
        content = open(path, 'r').read().decode('utf-8')

        def _nb_of_lines(start, end):
            return len(content[start:end].splitlines())

        def _build(replacement):
            offset = int(replacement.attrib['offset'])
            length = int(replacement.attrib['length'])
            last_line = content.rfind('\n', 0, offset)
            return {
                'replacement': replacement.text,
                'char_offset': offset,
                'char_length': length,
                'line': _nb_of_lines(0, offset),
                'line_offset': last_line != -1 and (offset - last_line) or 0,
                'lines_modified': _nb_of_lines(offset, offset + length),
            }

        return [
            _build(replacement)
            for replacement in ET.fromstring(clang_output).findall('replacement')
        ]
