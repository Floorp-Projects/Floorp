#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
import sys
import os
import glob

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.base.log import ERROR
from mozharness.base.transfer import TransferMixin
from mozharness.mozilla.mock import MockMixin


class OpenH264Build(MockMixin, TransferMixin, VCSScript):
    all_actions = [
        'clobber',
        'checkout-sources',
        'build',
        'test',
        'package',
        'upload',
    ]

    default_actions = [
        'checkout-sources',
        'build',
        'test',
        'package',
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
        [["--64"], {
            "dest": "64bit",
            "action": "store_true",
            "help": "Do a 64-bit build",
            "default": True,
        }],
        [["--32"], {
            "dest": "64bit",
            "action": "store_false",
            "help": "Do a 32-bit build",
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
    ]

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):

        # Default configuration
        default_config = {
            'debug_build': False,
            'mock_target': 'mozilla-centos6-x86_64',
            'mock_packages': ['make', 'git', 'nasm', 'glibc-devel.i686', 'libstdc++-devel.i686', 'zip', 'yasm'],
            'mock_files': [],
            'upload_ssh_key': os.path.expanduser("~/.ssh/ffxbld_rsa"),
            'upload_ssh_user': 'ffxbld',
            'upload_ssh_host': 'stage.mozilla.org',
            'upload_path_base': '/home/ffxbld/openh264',
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

    def query_package_name(self):
        if self.config['64bit']:
            bits = '64'
        else:
            bits = '32'

        version = self.config['revision']

        if sys.platform == 'linux2':
            if self.config.get('operating_system') == 'android':
                return 'openh264-android-{version}.zip'.format(version=version, bits=bits)
            else:
                return 'openh264-linux{bits}-{version}.zip'.format(version=version, bits=bits)
        elif sys.platform == 'darwin':
            return 'openh264-macosx{bits}-{version}.zip'.format(version=version, bits=bits)
        elif sys.platform == 'win32':
            return 'openh264-win{bits}-{version}.zip'.format(version=version, bits=bits)
        self.fatal("can't determine platform")

    def query_make_params(self):
        retval = []
        if self.config['debug_build']:
            retval.append('BUILDTYPE=Debug')

        if self.config['64bit']:
            retval.append('ENABLE64BIT=Yes')
        else:
            retval.append('ENABLE64BIT=No')

        if "operating_system" in self.config:
            retval.append("OS=%s" % self.config['operating_system'])

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

    def run_make(self, target):
        cmd = ['make', target] + self.query_make_params()
        dirs = self.query_abs_dirs()
        repo_dir = os.path.join(dirs['abs_work_dir'], 'src')
        return self.run_command(cmd, cwd=repo_dir)

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
        to_package = [os.path.basename(f) for f in glob.glob(os.path.join(srcdir, "*gmpopenh264*"))]
        cmd = ['zip', package_file] + to_package
        retval = self.run_command(cmd, cwd=srcdir)
        if retval != 0:
            self.fatal("couldn't make package")
        self.copy_to_upload_dir(package_file)

    def upload(self):
        if self.config['use_mock']:
            self.disable_mock()
        dirs = self.query_abs_dirs()
        self.rsync_upload_directory(
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
