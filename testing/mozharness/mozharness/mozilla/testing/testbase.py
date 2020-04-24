#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import copy
import os
import platform
from six.moves import urllib
import json
from six.moves.urllib.parse import urlparse, ParseResult

from mozharness.base.errors import BaseErrorList
from mozharness.base.log import FATAL, WARNING
from mozharness.base.python import (
    ResourceMonitoringMixin,
    VirtualenvMixin,
    virtualenv_config_options,
)
from mozharness.mozilla.automation import AutomationMixin, TBPL_WARNING
from mozharness.mozilla.structuredlog import StructuredOutputParser
from mozharness.mozilla.testing.unittest import DesktopUnittestOutputParser
from mozharness.mozilla.testing.try_tools import TryToolsMixin, try_config_options
from mozharness.mozilla.testing.verify_tools import VerifyToolsMixin, verify_config_options
from mozharness.mozilla.tooltool import TooltoolMixin

from mozharness.lib.python.authentication import get_credentials

INSTALLER_SUFFIXES = ('.apk',  # Android
                      '.tar.bz2', '.tar.gz',  # Linux
                      '.dmg',  # Mac
                      '.installer-stub.exe', '.installer.exe', '.exe', '.zip',  # Windows
                      )

# https://dxr.mozilla.org/mozilla-central/source/testing/config/tooltool-manifests
TOOLTOOL_PLATFORM_DIR = {
    'linux':   'linux32',
    'linux64': 'linux64',
    'win32':   'win32',
    'win64':   'win32',
    'macosx':  'macosx64',
}


testing_config_options = [
    [["--installer-url"],
     {"action": "store",
      "dest": "installer_url",
      "default": None,
      "help": "URL to the installer to install",
      }],
    [["--installer-path"],
     {"action": "store",
      "dest": "installer_path",
      "default": None,
      "help": "Path to the installer to install. "
      "This is set automatically if run with --download-and-extract.",
      }],
    [["--binary-path"],
     {"action": "store",
      "dest": "binary_path",
      "default": None,
      "help": "Path to installed binary.  This is set automatically if run with --install.",
      }],
    [["--exe-suffix"],
     {"action": "store",
      "dest": "exe_suffix",
      "default": None,
      "help": "Executable suffix for binaries on this platform",
      }],
    [["--test-url"],
     {"action": "store",
      "dest": "test_url",
      "default": None,
      "help": "URL to the zip file containing the actual tests",
      }],
    [["--test-packages-url"],
     {"action": "store",
      "dest": "test_packages_url",
      "default": None,
      "help": "URL to a json file describing which tests archives to download",
      }],
    [["--jsshell-url"],
     {"action": "store",
      "dest": "jsshell_url",
      "default": None,
      "help": "URL to the jsshell to install",
      }],
    [["--download-symbols"],
     {"action": "store",
      "dest": "download_symbols",
      "type": "choice",
      "choices": ['ondemand', 'true'],
      "help": "Download and extract crash reporter symbols.",
      }],
] + copy.deepcopy(virtualenv_config_options) \
  + copy.deepcopy(try_config_options) \
  + copy.deepcopy(verify_config_options)


# TestingMixin {{{1
class TestingMixin(VirtualenvMixin, AutomationMixin, ResourceMonitoringMixin,
                   TooltoolMixin, TryToolsMixin, VerifyToolsMixin):
    """
    The steps to identify + download the proper bits for [browser] unit
    tests and Talos.
    """

    installer_url = None
    installer_path = None
    binary_path = None
    test_url = None
    test_packages_url = None
    symbols_url = None
    symbols_path = None
    jsshell_url = None
    minidump_stackwalk_path = None

    def query_build_dir_url(self, file_name):
        """
        Resolve a file name to a potential url in the build upload directory where
        that file can be found.
        """
        if self.test_packages_url:
            reference_url = self.test_packages_url
        elif self.installer_url:
            reference_url = self.installer_url
        else:
            self.fatal("Can't figure out build directory urls without an installer_url "
                       "or test_packages_url!")

        reference_url = urllib.parse.unquote(reference_url)
        parts = list(urlparse(reference_url))

        last_slash = parts[2].rfind('/')
        parts[2] = '/'.join([parts[2][:last_slash], file_name])

        url = ParseResult(*parts).geturl()

        return url

    def query_prefixed_build_dir_url(self, suffix):
        """Resolve a file name prefixed with platform and build details to a potential url
        in the build upload directory where that file can be found.
        """
        if self.test_packages_url:
            reference_suffixes = ['.test_packages.json']
            reference_url = self.test_packages_url
        elif self.installer_url:
            reference_suffixes = INSTALLER_SUFFIXES
            reference_url = self.installer_url
        else:
            self.fatal("Can't figure out build directory urls without an installer_url "
                       "or test_packages_url!")

        url = None
        for reference_suffix in reference_suffixes:
            if reference_url.endswith(reference_suffix):
                url = reference_url[:-len(reference_suffix)] + suffix
                break

        return url

    def query_symbols_url(self, raise_on_failure=False):
        if self.symbols_url:
            return self.symbols_url

        elif self.installer_url:
            symbols_url = self.query_prefixed_build_dir_url('.crashreporter-symbols.zip')

            # Check if the URL exists. If not, use none to allow mozcrash to auto-check for symbols
            try:
                if symbols_url:
                    self._urlopen(symbols_url, timeout=120)
                    self.symbols_url = symbols_url
            except Exception as ex:
                self.warning("Cannot open symbols url %s (installer url: %s): %s" %
                             (symbols_url, self.installer_url, ex))
                if raise_on_failure:
                    raise

        # If no symbols URL can be determined let minidump_stackwalk query the symbols.
        # As of now this only works for Nightly and release builds.
        if not self.symbols_url:
            self.warning("No symbols_url found. Let minidump_stackwalk query for symbols.")

        return self.symbols_url

    def _pre_config_lock(self, rw_config):
        for i, (target_file, target_dict) in enumerate(rw_config.all_cfg_files_and_dicts):
            if 'developer_config' in target_file:
                self._developer_mode_changes(rw_config)

    def _developer_mode_changes(self, rw_config):
        """ This function is called when you append the config called
            developer_config.py. This allows you to run a job
            outside of the Release Engineering infrastructure.

            What this functions accomplishes is:
            * --installer-url is set
            * --test-url is set if needed
            * every url is substituted by another external to the
                Release Engineering network
        """
        c = self.config
        orig_config = copy.deepcopy(c)
        self.actions = tuple(rw_config.actions)

        def _replace_url(url, changes):
            for from_, to_ in changes:
                if url.startswith(from_):
                    new_url = url.replace(from_, to_)
                    self.info("Replacing url %s -> %s" % (url, new_url))
                    return new_url
            return url

        if c.get("installer_url") is None:
            self.exception("You must use --installer-url with developer_config.py")
        if c.get("require_test_zip"):
            if not c.get('test_url') and not c.get('test_packages_url'):
                self.exception("You must use --test-url or --test-packages-url with "
                               "developer_config.py")

        c["installer_url"] = _replace_url(c["installer_url"], c["replace_urls"])
        if c.get("test_url"):
            c["test_url"] = _replace_url(c["test_url"], c["replace_urls"])
        if c.get("test_packages_url"):
            c["test_packages_url"] = _replace_url(c["test_packages_url"], c["replace_urls"])

        for key, value in self.config.items():
            if type(value) == str and value.startswith("http"):
                self.config[key] = _replace_url(value, c["replace_urls"])

        # Any changes to c means that we need credentials
        if not c == orig_config:
            get_credentials()

    def _urlopen(self, url, **kwargs):
        '''
        This function helps dealing with downloading files while outside
        of the releng network.
        '''
        # Code based on http://code.activestate.com/recipes/305288-http-basic-authentication
        def _urlopen_basic_auth(url, **kwargs):
            self.info("We want to download this file %s" % url)
            if not hasattr(self, "https_username"):
                self.info("NOTICE: Files downloaded from outside of "
                          "Release Engineering network require LDAP "
                          "credentials.")

            self.https_username, self.https_password = get_credentials()
            # This creates a password manager
            passman = urllib.request.HTTPPasswordMgrWithDefaultRealm()
            # Because we have put None at the start it will use this username/password
            # combination from here on
            passman.add_password(None, url, self.https_username, self.https_password)
            authhandler = urllib.request.HTTPBasicAuthHandler(passman)

            return urllib.request.build_opener(authhandler).open(url, **kwargs)

        # If we have the developer_run flag enabled then we will switch
        # URLs to the right place and enable http authentication
        if "developer_config.py" in self.config["config_files"]:
            return _urlopen_basic_auth(url, **kwargs)
        else:
            return urllib.request.urlopen(url, **kwargs)

    def _query_binary_version(self, regex, cmd):
        output = self.get_output_from_command(cmd, silent=False)
        return regex.search(output).group(0)

    def preflight_download_and_extract(self):
        message = ""
        if not self.installer_url:
            message += """installer_url isn't set!

You can set this by specifying --installer-url URL
"""
        if (self.config.get("require_test_zip") and
            not self.test_url and
            not self.test_packages_url):
            message += """test_url isn't set!

You can set this by specifying --test-url URL
"""
        if message:
            self.fatal(message + "Can't run download-and-extract... exiting")

    def _read_packages_manifest(self):
        dirs = self.query_abs_dirs()
        source = self.download_file(self.test_packages_url,
                                    parent_dir=dirs['abs_work_dir'],
                                    error_level=FATAL)

        with self.opened(os.path.realpath(source)) as (fh, err):
            package_requirements = json.load(fh)
            if not package_requirements or err:
                self.fatal("There was an error reading test package requirements from %s "
                           "requirements: `%s` - error: `%s`" % (source,
                                                                 package_requirements or 'None',
                                                                 err or 'No error'))
        return package_requirements

    def _download_test_packages(self, suite_categories, extract_dirs):
        # Some platforms define more suite categories/names than others.
        # This is a difference in the convention of the configs more than
        # to how these tests are run, so we pave over these differences here.
        aliases = {
            'mochitest-chrome': 'mochitest',
            'mochitest-media': 'mochitest',
            'mochitest-plain': 'mochitest',
            'mochitest-plain-gpu': 'mochitest',
            'mochitest-webgl1-core': 'mochitest',
            'mochitest-webgl1-ext': 'mochitest',
            'mochitest-webgl2-core': 'mochitest',
            'mochitest-webgl2-ext': 'mochitest',
            'mochitest-webgl2-deqp': 'mochitest',
            'mochitest-webgpu': 'mochitest',
            'geckoview': 'mochitest',
            'geckoview-junit': 'mochitest',
            'reftest-qr': 'reftest',
            'jsreftest': 'reftest',
            'crashtest': 'reftest',
            'reftest-debug': 'reftest',
            'jsreftest-debug': 'reftest',
            'crashtest-debug': 'reftest',
        }
        suite_categories = [aliases.get(name, name) for name in suite_categories]

        dirs = self.query_abs_dirs()
        test_install_dir = dirs.get('abs_test_install_dir',
                                    os.path.join(dirs['abs_work_dir'], 'tests'))
        self.mkdir_p(test_install_dir)
        package_requirements = self._read_packages_manifest()
        target_packages = []
        for category in suite_categories:
            if category in package_requirements:
                target_packages.extend(package_requirements[category])
            else:
                # If we don't harness specific requirements, assume the common zip
                # has everything we need to run tests for this suite.
                target_packages.extend(package_requirements['common'])

        # eliminate duplicates -- no need to download anything twice
        target_packages = list(set(target_packages))
        self.info("Downloading packages: %s for test suite categories: %s" %
                  (target_packages, suite_categories))
        for file_name in target_packages:
            target_dir = test_install_dir
            unpack_dirs = extract_dirs

            if "common.tests" in file_name and isinstance(unpack_dirs, list):
                # Ensure that the following files are always getting extracted
                required_files = ["mach",
                                  "mozinfo.json",
                                  ]
                for req_file in required_files:
                    if req_file not in unpack_dirs:
                        self.info("Adding '{}' for extraction from common.tests archive"
                                  .format(req_file))
                        unpack_dirs.append(req_file)

            if "jsshell-" in file_name or file_name == "target.jsshell.zip":
                self.info("Special-casing the jsshell zip file")
                unpack_dirs = None
                target_dir = dirs['abs_test_bin_dir']

            url = self.query_build_dir_url(file_name)
            self.download_unpack(url, target_dir,
                                 extract_dirs=unpack_dirs)

    def _download_test_zip(self, extract_dirs=None):
        dirs = self.query_abs_dirs()
        test_install_dir = dirs.get('abs_test_install_dir',
                                    os.path.join(dirs['abs_work_dir'], 'tests'))
        self.download_unpack(self.test_url, test_install_dir,
                             extract_dirs=extract_dirs)

    def structured_output(self, suite_category):
        """Defines whether structured logging is in use in this configuration. This
        may need to be replaced with data from a different config at the resolution
        of bug 1070041 and related bugs.
        """
        return ('structured_suites' in self.config and
                suite_category in self.config['structured_suites'])

    def get_test_output_parser(self, suite_category, strict=False,
                               fallback_parser_class=DesktopUnittestOutputParser,
                               **kwargs):
        """Derive and return an appropriate output parser, either the structured
        output parser or a fallback based on the type of logging in use as determined by
        configuration.
        """
        if not self.structured_output(suite_category):
            if fallback_parser_class is DesktopUnittestOutputParser:
                return DesktopUnittestOutputParser(suite_category=suite_category, **kwargs)
            return fallback_parser_class(**kwargs)
        self.info("Structured output parser in use for %s." % suite_category)
        return StructuredOutputParser(suite_category=suite_category, strict=strict, **kwargs)

    def _download_installer(self):
        file_name = None
        if self.installer_path:
            file_name = self.installer_path
        dirs = self.query_abs_dirs()
        source = self.download_file(self.installer_url,
                                    file_name=file_name,
                                    parent_dir=dirs['abs_work_dir'],
                                    error_level=FATAL)
        self.installer_path = os.path.realpath(source)

    def _download_and_extract_symbols(self):
        dirs = self.query_abs_dirs()
        if self.config.get('download_symbols') == 'ondemand':
            self.symbols_url = self.query_symbols_url()
            self.symbols_path = self.symbols_url
            return

        else:
            # In the case for 'ondemand', we're OK to proceed without getting a hold of the
            # symbols right this moment, however, in other cases we need to at least retry
            # before being unable to proceed (e.g. debug tests need symbols)
            self.symbols_url = self.retry(
                action=self.query_symbols_url,
                kwargs={'raise_on_failure': True},
                sleeptime=20,
                error_level=FATAL,
                error_message="We can't proceed without downloading symbols.",
            )
            if not self.symbols_path:
                self.symbols_path = os.path.join(dirs['abs_work_dir'], 'symbols')

            if self.symbols_url:
                self.download_unpack(self.symbols_url, self.symbols_path)

    def download_and_extract(self, extract_dirs=None, suite_categories=None):
        """
        download and extract test zip / download installer
        """
        # Swap plain http for https when we're downloading from ftp
        # See bug 957502 and friends
        from_ = "http://ftp.mozilla.org"
        to_ = "https://ftp-ssl.mozilla.org"
        for attr in 'symbols_url', 'installer_url', 'test_packages_url', 'test_url':
            url = getattr(self, attr)
            if url and url.startswith(from_):
                new_url = url.replace(from_, to_)
                self.info("Replacing url %s -> %s" % (url, new_url))
                setattr(self, attr, new_url)

        if 'test_url' in self.config:
            # A user has specified a test_url directly, any test_packages_url will
            # be ignored.
            if self.test_packages_url:
                self.error('Test data will be downloaded from "%s", the specified test '
                           ' package data at "%s" will be ignored.' %
                           (self.config.get('test_url'), self.test_packages_url))

            self._download_test_zip(extract_dirs)
        else:
            if not self.test_packages_url:
                # The caller intends to download harness specific packages, but doesn't know
                # where the packages manifest is located. This is the case when the
                # test package manifest isn't set as a property, which is true
                # for some self-serve jobs and platforms using parse_make_upload.
                self.test_packages_url = self.query_prefixed_build_dir_url('.test_packages.json')

            suite_categories = suite_categories or ['common']
            self._download_test_packages(suite_categories, extract_dirs)

        self._download_installer()
        if self.config.get('download_symbols'):
            self._download_and_extract_symbols()

    # create_virtualenv is in VirtualenvMixin.

    def preflight_install(self):
        if not self.installer_path:
            if self.config.get('installer_path'):
                self.installer_path = self.config['installer_path']
            else:
                self.fatal("""installer_path isn't set!

You can set this by:

1. specifying --installer-path PATH, or
2. running the download-and-extract action
""")
        if not self.is_python_package_installed("mozInstall"):
            self.fatal("""Can't call install() without mozinstall!
Did you run with --create-virtualenv? Is mozinstall in virtualenv_modules?""")

    def install_app(self, app=None, target_dir=None, installer_path=None):
        """ Dependent on mozinstall """
        # install the application
        cmd = [self.query_python_path("mozinstall")]
        if app:
            cmd.extend(['--app', app])
        # Remove the below when we no longer need to support mozinstall 0.3
        self.info("Detecting whether we're running mozinstall >=1.0...")
        output = self.get_output_from_command(cmd + ['-h'])
        if '--source' in output:
            cmd.append('--source')
        # End remove
        dirs = self.query_abs_dirs()
        if not target_dir:
            target_dir = dirs.get('abs_app_install_dir',
                                  os.path.join(dirs['abs_work_dir'],
                                               'application'))
        self.mkdir_p(target_dir)
        if not installer_path:
            installer_path = self.installer_path
        cmd.extend([installer_path,
                    '--destination', target_dir])
        # TODO we'll need some error checking here
        return self.get_output_from_command(cmd, halt_on_failure=True,
                                            fatal_exit_code=3)

    def install(self):
        self.binary_path = self.install_app(app=self.config.get('application'))

    def uninstall_app(self, install_dir=None):
        """ Dependent on mozinstall """
        # uninstall the application
        cmd = self.query_exe("mozuninstall",
                             default=self.query_python_path("mozuninstall"),
                             return_type="list")
        dirs = self.query_abs_dirs()
        if not install_dir:
            install_dir = dirs.get('abs_app_install_dir',
                                   os.path.join(dirs['abs_work_dir'],
                                                'application'))
        cmd.append(install_dir)
        # TODO we'll need some error checking here
        self.get_output_from_command(cmd, halt_on_failure=True,
                                     fatal_exit_code=3)

    def uninstall(self):
        self.uninstall_app()

    def query_minidump_stackwalk(self, manifest=None):
        if self.minidump_stackwalk_path:
            return self.minidump_stackwalk_path

        minidump_stackwalk_path = None

        if 'MOZ_FETCHES_DIR' in os.environ:
            minidump_stackwalk_path = os.path.join(
                os.environ['MOZ_FETCHES_DIR'],
                'minidump_stackwalk',
                'minidump_stackwalk')

            if self.platform_name() in ('win32', 'win64'):
                minidump_stackwalk_path += '.exe'

        if not minidump_stackwalk_path or not os.path.isfile(minidump_stackwalk_path):
            self.error("minidump_stackwalk path was not fetched?")
            # don't burn the job but we should at least turn them orange so it is caught
            self.record_status(TBPL_WARNING, WARNING)
            return None

        self.minidump_stackwalk_path = minidump_stackwalk_path
        return self.minidump_stackwalk_path

    def query_options(self, *args, **kwargs):
        if "str_format_values" in kwargs:
            str_format_values = kwargs.pop("str_format_values")
        else:
            str_format_values = {}

        arguments = []

        for arg in args:
            if arg is not None:
                arguments.extend(argument % str_format_values for argument in arg)

        return arguments

    def query_tests_args(self, *args, **kwargs):
        if "str_format_values" in kwargs:
            str_format_values = kwargs.pop("str_format_values")
        else:
            str_format_values = {}

        arguments = []

        for arg in reversed(args):
            if arg:
                arguments.append("--")
                arguments.extend(argument % str_format_values for argument in arg)
                break

        return arguments

    def _run_cmd_checks(self, suites):
        if not suites:
            return
        dirs = self.query_abs_dirs()
        for suite in suites:
            # XXX platform.architecture() may give incorrect values for some
            # platforms like mac as excutable files may be universal
            # files containing multiple architectures
            # NOTE 'enabled' is only here while we have unconsolidated configs
            if not suite['enabled']:
                continue
            if suite.get('architectures'):
                arch = platform.architecture()[0]
                if arch not in suite['architectures']:
                    continue
            cmd = suite['cmd']
            name = suite['name']
            self.info("Running pre test command %(name)s with '%(cmd)s'"
                      % {'name': name, 'cmd': ' '.join(cmd)})
            self.run_command(cmd,
                             cwd=dirs['abs_work_dir'],
                             error_list=BaseErrorList,
                             halt_on_failure=suite['halt_on_failure'],
                             fatal_exit_code=suite.get('fatal_exit_code', 3))

    def preflight_run_tests(self):
        """preflight commands for all tests"""
        c = self.config
        if c.get('run_cmd_checks_enabled'):
            self._run_cmd_checks(c.get('preflight_run_cmd_suites', []))
        elif c.get('preflight_run_cmd_suites'):
            self.warning("Proceeding without running prerun test commands."
                         " These are often OS specific and disabling them may"
                         " result in spurious test results!")

    def postflight_run_tests(self):
        """preflight commands for all tests"""
        c = self.config
        if c.get('run_cmd_checks_enabled'):
            self._run_cmd_checks(c.get('postflight_run_cmd_suites', []))
