#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import sys
import tarfile
import tempfile

import mozinfo
from mozharness.base.script import (
    PreScriptAction,
    PostScriptAction,
)
from mozharness.mozilla.tooltool import TooltoolMixin

code_coverage_config_options = [
    [["--code-coverage"],
     {"action": "store_true",
      "dest": "code_coverage",
      "default": False,
      "help": "Whether gcov c++ code coverage should be run."
      }],
    [["--disable-ccov-upload"],
     {"action": "store_true",
      "dest": "disable_ccov_upload",
      "default": False,
      "help": "Whether test run should package and upload code coverage data."
      }],
    [["--jsd-code-coverage"],
     {"action": "store_true",
      "dest": "jsd_code_coverage",
      "default": False,
      "help": "Whether JSDebugger code coverage should be run."
      }],
]


class CodeCoverageMixin(object):
    """
    Mixin for setting GCOV_PREFIX during test execution, packaging up
    the resulting .gcda files and uploading them to blobber.
    """
    gcov_dir = None
    jsvm_dir = None
    prefix = None

    @property
    def code_coverage_enabled(self):
        try:
            if self.config.get('code_coverage'):
                return True

            # XXX workaround because bug 1110465 is hard
            return 'ccov' in self.buildbot_config['properties']['stage_platform']
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def ccov_upload_disabled(self):
        try:
            if self.config.get('disable_ccov_upload'):
                return True
            return False
        except (AttributeError, KeyError, TypeError):
            return False

    @property
    def jsd_code_coverage_enabled(self):
        try:
            if self.config.get('jsd_code_coverage'):
                return True

            # XXX workaround because bug 1110465 is hard
            return 'jsdcov' in self.buildbot_config['properties']['stage_platform']
        except (AttributeError, KeyError, TypeError):
            return False

    @PreScriptAction('run-tests')
    def _set_gcov_prefix(self, action):
        if not self.code_coverage_enabled:
            return

        if mozinfo.os == 'linux':
            self.prefix = '/builds/worker/workspace/build/src/'
            strip_count = self.prefix.count('/')
        elif mozinfo.os == 'win':
            self.prefix = 'z:/build/build/src/'
            # Add 1 as on Windows the path where the compiler tries to write the
            # gcda files has an additional 'obj-firefox' component.
            strip_count = self.prefix.count('/') + 1

        os.environ['GCOV_PREFIX_STRIP'] = str(strip_count)

        # Set the GCOV directory.
        self.gcov_dir = tempfile.mkdtemp()
        os.environ['GCOV_PREFIX'] = self.gcov_dir

        # Set JSVM directory.
        self.jsvm_dir = tempfile.mkdtemp()
        os.environ['JS_CODE_COVERAGE_OUTPUT_DIR'] = self.jsvm_dir

        # Install grcov on the test machine
        # Get the path to the build machines gcno files.
        self.url_to_gcno = self.query_build_dir_url('target.code-coverage-gcno.zip')
        dirs = self.query_abs_dirs()

        # Create the grcov directory, get the tooltool manifest, and finally
        # download and unpack the grcov binary.
        self.grcov_dir = tempfile.mkdtemp()

        if mozinfo.os == 'linux':
            platform = 'linux64'
            tar_file = 'grcov-linux-standalone-x86_64.tar.bz2'
        elif mozinfo.os == 'win':
            platform = 'win32'
            tar_file = 'grcov-win-i686.tar.bz2'

        manifest = os.path.join(dirs.get('abs_test_install_dir', os.path.join(dirs['abs_work_dir'], 'tests')), \
            'config/tooltool-manifests/%s/ccov.manifest' % platform)

        tooltool_path = self._fetch_tooltool_py()
        cmd = [sys.executable, tooltool_path, '--url', 'https://tooltool.mozilla-releng.net/', 'fetch', \
            '-m', manifest, '-o', '-c', '/builds/worker/tooltool-cache']
        self.run_command(cmd, cwd=self.grcov_dir)

        with tarfile.open(os.path.join(self.grcov_dir, tar_file)) as tar:
            tar.extractall(self.grcov_dir)

    @PostScriptAction('run-tests')
    def _package_coverage_data(self, action, success=None):
        if self.jsd_code_coverage_enabled:
            # Setup the command for compression
            dirs = self.query_abs_dirs()
            jsdcov_dir = dirs['abs_blob_upload_dir']
            zipFile = os.path.join(jsdcov_dir, "jsdcov_artifacts.zip")
            command = ["zip", "-r", "-q", zipFile, ".", "-i", "jscov*.json"]

            self.info("Beginning compression of JSDCov artifacts...")
            self.run_command(command, cwd=jsdcov_dir)

            # Delete already compressed JSCov artifacts.
            for filename in os.listdir(jsdcov_dir):
                if filename.startswith("jscov") and filename.endswith(".json"):
                    os.remove(os.path.join(jsdcov_dir, filename))

            self.info("Completed compression of JSDCov artifacts!")
            self.info("Path to JSDCov compressed artifacts: " + zipFile)

        if not self.code_coverage_enabled:
            return

        del os.environ['GCOV_PREFIX_STRIP']
        del os.environ['GCOV_PREFIX']
        del os.environ['JS_CODE_COVERAGE_OUTPUT_DIR']

        if not self.ccov_upload_disabled:
            dirs = self.query_abs_dirs()

            # Zip gcda files (will be given in input to grcov).
            file_path_gcda = os.path.join(os.getcwd(), 'code-coverage-gcda.zip')
            self.run_command(['zip', '-q', '-0', '-r', file_path_gcda, '.'], cwd=self.gcov_dir)

            # Package JSVM coverage data.
            file_path_jsvm = os.path.join(dirs['abs_blob_upload_dir'], 'code-coverage-jsvm.zip')
            self.run_command(['zip', '-r', '-q', file_path_jsvm, '.'], cwd=self.jsvm_dir)

            # GRCOV post-processing
            # Download the gcno fom the build machine.
            self.download_file(self.url_to_gcno, file_name=None, parent_dir=self.grcov_dir)

            # Run grcov on the zipped .gcno and .gcda files.
            grcov_command = [
                os.path.join(self.grcov_dir, 'grcov'),
                '-t', 'lcov',
                '-p', self.prefix,
                '--ignore-dir', 'gcc',
                os.path.join(self.grcov_dir, 'target.code-coverage-gcno.zip'), file_path_gcda
            ]

            if mozinfo.os == 'win':
                grcov_command += ['--llvm']

            # 'grcov_output' will be a tuple, the first variable is the path to the lcov output,
            # the other is the path to the standard error output.
            grcov_output, _ = self.get_output_from_command(
                grcov_command,
                silent=True,
                save_tmpfiles=True,
                return_type='files',
                throw_exception=True,
            )
            output_file_name = 'grcov_lcov_output.info'
            shutil.move(grcov_output, os.path.join(self.grcov_dir, output_file_name))

            # Zip the grcov output and upload it.
            self.run_command(
                ['zip', '-q', os.path.join(dirs['abs_blob_upload_dir'], 'code-coverage-grcov.zip'), output_file_name],
                cwd=self.grcov_dir
            )

        shutil.rmtree(self.gcov_dir)
        shutil.rmtree(self.jsvm_dir)
        shutil.rmtree(self.grcov_dir)
