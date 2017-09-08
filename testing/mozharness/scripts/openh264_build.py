#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import sys
import os
import glob
import re

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
import mozharness
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.base.log import ERROR
from mozharness.base.transfer import TransferMixin
from mozharness.mozilla.mock import MockMixin
from mozharness.mozilla.tooltool import TooltoolMixin


external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)


class OpenH264Build(MockMixin, TransferMixin, VCSScript, TooltoolMixin):
    all_actions = [
        'clobber',
        'get-tooltool',
        'checkout-sources',
        'build',
        'test',
        'package',
        'dump-symbols',
        'upload',
    ]

    default_actions = [
        'get-tooltool',
        'checkout-sources',
        'build',
        'test',
        'package',
        'dump-symbols',
        'upload',
    ]

    config_options = [
        [["--repo"], {
            "dest": "repo",
            "help": "OpenH264 repository to use",
            "default": "https://github.com/cisco/openh264.git"
        }],
        [["--rev"], {
            "dest": "revision",
            "help": "revision to checkout",
            "default": "master"
        }],
        [["--debug"], {
            "dest": "debug_build",
            "action": "store_true",
            "help": "Do a debug build",
        }],
        [["--arch"], {
            "dest": "arch",
            "help": "Arch type to use (x64, x86, arm, or aarch64)",
        }],
        [["--os"], {
            "dest": "operating_system",
            "help": "Specify the operating system to build for",
        }],
        [["--use-mock"], {
            "dest": "use_mock",
            "help": "use mock to set up build environment",
            "action": "store_true",
            "default": False,
        }],
        [["--use-yasm"], {
            "dest": "use_yasm",
            "help": "use yasm instead of nasm",
            "action": "store_true",
            "default": False,
        }],
        [["--avoid-avx2"], {
            "dest": "avoid_avx2",
            "help": "Pass HAVE_AVX2='false' through to Make to support older nasm",
            "action": "store_true",
            "default": False,
        }]
    ]

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):

        # Default configuration
        default_config = {
            'debug_build': False,
            'upload_ssh_key': "~/.ssh/ffxbld_rsa",
            'upload_ssh_user': 'ffxbld',
            'upload_ssh_host': 'upload.ffxbld.productdelivery.prod.mozaws.net',
            'upload_path_base': '/tmp/openh264',
            'use_yasm': False,
        }
        default_config.update(config)

        VCSScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions,
        )

        if self.config['use_mock']:
            self.setup_mock()
            self.enable_mock()

    def get_tooltool(self):
        c = self.config
        if not c.get('tooltool_manifest_file'):
            self.info("Skipping tooltool fetching since no tooltool manifest")
            return
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs['abs_work_dir'])
        manifest = os.path.join(dirs['base_work_dir'],
                                'scripts', 'configs',
                                'openh264', 'tooltool-manifests',
                                c['tooltool_manifest_file'])
        self.info("Getting tooltool files from manifest (%s)" % manifest)
        try:
            self.tooltool_fetch(
                manifest=manifest,
                output_dir=dirs['abs_work_dir'],
                cache=c.get('tooltool_cache')
            )
        except KeyError:
            self.error('missing a required key.')

    def query_package_name(self):
        if self.config['arch'] == "x64":
            bits = '64'
        else:
            bits = '32'
        version = self.config['revision']

        if sys.platform == 'linux2':
            if self.config.get('operating_system') == 'android':
                return 'openh264-android-{arch}-{version}.zip'.format(
                    version=version, arch=self.config['arch'])
            else:
                return 'openh264-linux{bits}-{version}.zip'.format(version=version, bits=bits)
        elif sys.platform == 'darwin':
            return 'openh264-macosx{bits}-{version}.zip'.format(version=version, bits=bits)
        elif sys.platform == 'win32':
            return 'openh264-win{bits}-{version}.zip'.format(version=version, bits=bits)
        self.fatal("can't determine platform")

    def query_make_params(self):
        dirs = self.query_abs_dirs()
        retval = []
        if self.config['debug_build']:
            retval.append('BUILDTYPE=Debug')

        if self.config['avoid_avx2']:
            retval.append('HAVE_AVX2=false')

        if self.config['arch'] in ('x64', 'aarch64'):
            retval.append('ENABLE64BIT=Yes')
        else:
            retval.append('ENABLE64BIT=No')

        if "operating_system" in self.config:
            retval.append("OS=%s" % self.config['operating_system'])
            if self.config["operating_system"] == "android":
                if self.config['arch'] == 'x86':
                    retval.append("ARCH=x86")
                elif self.config['arch'] == 'aarch64':
                    retval.append("ARCH=arm64")
                else:
                    retval.append("ARCH=arm")
                retval.append('TARGET=invalid')
                retval.append('NDKLEVEL=%s' % self.config['min_sdk'])
                retval.append('NDKROOT=%s/android-ndk-r11c' % dirs['abs_work_dir'])

        if self.config['use_yasm']:
            retval.append('ASM=yasm')

        return retval

    def query_upload_ssh_key(self):
        return self.config['upload_ssh_key']

    def query_upload_ssh_host(self):
        return self.config['upload_ssh_host']

    def query_upload_ssh_user(self):
        return self.config['upload_ssh_user']

    def query_upload_ssh_path(self):
        return "%s/%s" % (self.config['upload_path_base'], self.config['revision'])

    def run_make(self, target, capture_output=False):
        cmd = ['make', target] + self.query_make_params()
        dirs = self.query_abs_dirs()
        repo_dir = os.path.join(dirs['abs_work_dir'], 'src')
        env = None
        if self.config.get('partial_env'):
            env = self.query_env(self.config['partial_env'])
        kwargs = dict(cwd=repo_dir, env=env)
        if capture_output:
            return self.get_output_from_command(cmd, **kwargs)
        else:
            return self.run_command(cmd, **kwargs)

    def checkout_sources(self):
        repo = self.config['repo']
        rev = self.config['revision']

        dirs = self.query_abs_dirs()
        repo_dir = os.path.join(dirs['abs_work_dir'], 'src')

        repos = [
            {'vcs': 'gittool', 'repo': repo, 'dest': repo_dir, 'revision': rev},
        ]

        # self.vcs_checkout already retries, so no need to wrap it in
        # self.retry. We set the error_level to ERROR to prevent it going fatal
        # so we can do our own handling here.
        retval = self.vcs_checkout_repos(repos, error_level=ERROR)
        if not retval:
            self.rmtree(repo_dir)
            self.fatal("Automation Error: couldn't clone repo", exit_code=4)

        # Checkout gmp-api
        # TODO: Nothing here updates it yet, or enforces versions!
        if not os.path.exists(os.path.join(repo_dir, 'gmp-api')):
            retval = self.run_make('gmp-bootstrap')
            if retval != 0:
                self.fatal("couldn't bootstrap gmp")
        else:
            self.info("skipping gmp bootstrap - we have it locally")

        # Checkout gtest
        # TODO: Requires svn!
        if not os.path.exists(os.path.join(repo_dir, 'gtest')):
            retval = self.run_make('gtest-bootstrap')
            if retval != 0:
                self.fatal("couldn't bootstrap gtest")
        else:
            self.info("skipping gtest bootstrap - we have it locally")

        return retval

    def build(self):
        retval = self.run_make('plugin')
        if retval != 0:
            self.fatal("couldn't build plugin")

    def package(self):
        dirs = self.query_abs_dirs()
        srcdir = os.path.join(dirs['abs_work_dir'], 'src')
        package_name = self.query_package_name()
        package_file = os.path.join(dirs['abs_work_dir'], package_name)
        if os.path.exists(package_file):
            os.unlink(package_file)
        to_package = []
        for f in glob.glob(os.path.join(srcdir, "*gmpopenh264*")):
            if not re.search(
                    "(?:lib)?gmpopenh264(?!\.\d)\.(?:dylib|so|dll|info)(?!\.\d)",
                    f):
                # Don't package unnecessary zip bloat
                # Blocks things like libgmpopenh264.2.dylib and libgmpopenh264.so.1
                self.log("Skipping packaging of {package}".format(package=f))
                continue
            to_package.append(os.path.basename(f))
        self.log("Packaging files %s" % to_package)
        cmd = ['zip', package_file] + to_package
        retval = self.run_command(cmd, cwd=srcdir)
        if retval != 0:
            self.fatal("couldn't make package")
        self.copy_to_upload_dir(package_file)

    def dump_symbols(self):
        dirs = self.query_abs_dirs()
        c = self.config
        srcdir = os.path.join(dirs['abs_work_dir'], 'src')
        package_name = self.run_make('echo-plugin-name', capture_output=True)
        if not package_name:
            self.fatal("failure running make")
        zip_package_name = self.query_package_name()
        if not zip_package_name[-4:] == ".zip":
            self.fatal("Unexpected zip_package_name")
        symbol_package_name = "{base}.symbols.zip".format(base=zip_package_name[:-4])
        symbol_zip_path = os.path.join(dirs['abs_upload_dir'], symbol_package_name)
        repo_dir = os.path.join(dirs['abs_work_dir'], 'src')
        env = None
        if self.config.get('partial_env'):
            env = self.query_env(self.config['partial_env'])
        kwargs = dict(cwd=repo_dir, env=env)
        dump_syms = os.path.join(dirs['abs_work_dir'], c['dump_syms_binary'])
        self.chmod(dump_syms, 0755)
        python = self.query_exe('python2.7')
        cmd = [python, os.path.join(external_tools_path, 'packagesymbols.py'),
               '--symbol-zip', symbol_zip_path,
               dump_syms, os.path.join(srcdir, package_name)]
        if self.config['use_mock']:
            self.disable_mock()
        self.run_command(cmd, **kwargs)
        if self.config['use_mock']:
            self.enable_mock()

    def upload(self):
        if self.config['use_mock']:
            self.disable_mock()
        dirs = self.query_abs_dirs()
        self.scp_upload_directory(
            dirs['abs_upload_dir'],
            self.query_upload_ssh_key(),
            self.query_upload_ssh_user(),
            self.query_upload_ssh_host(),
            self.query_upload_ssh_path(),
        )
        if self.config['use_mock']:
            self.enable_mock()

    def test(self):
        retval = self.run_make('test')
        if retval != 0:
            self.fatal("test failures")


# main {{{1
if __name__ == '__main__':
    myScript = OpenH264Build()
    myScript.run_and_exit()
