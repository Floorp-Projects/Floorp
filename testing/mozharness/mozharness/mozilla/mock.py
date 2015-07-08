#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Code to integrate with mock
"""

import os.path
import hashlib
import subprocess
import os

ERROR_MSGS = {
    'undetermined_buildroot_lock': 'buildroot_lock_path does not exist.\
Nothing to remove.'
}



# MockMixin {{{1
class MockMixin(object):
    """Provides methods to setup and interact with mock environments.
    https://wiki.mozilla.org/ReleaseEngineering/Applications/Mock

    This is dependent on ScriptMixin
    """
    done_mock_setup = False
    mock_enabled = False
    default_mock_target = None

    def init_mock(self, mock_target):
        "Initialize mock environment defined by `mock_target`"
        cmd = ['mock_mozilla', '-r', mock_target, '--init']
        return super(MockMixin, self).run_command(cmd, halt_on_failure=True,
                                                  fatal_exit_code=3)

    def install_mock_packages(self, mock_target, packages):
        "Install `packages` into mock environment `mock_target`"
        cmd = ['mock_mozilla', '-r', mock_target, '--install'] + packages
        # TODO: parse output to see if packages actually were installed
        return super(MockMixin, self).run_command(cmd, halt_on_failure=True,
                                                  fatal_exit_code=3)

    def delete_mock_files(self, mock_target, files):
        """Delete files from the mock environment `mock_target`. `files` should
        be an iterable of 2-tuples: (src, dst). Only the dst component is
        deleted."""
        cmd_base = ['mock_mozilla', '-r', mock_target, '--shell']
        for src, dest in files:
            cmd = cmd_base + ['rm -rf %s' % dest]
            super(MockMixin, self).run_command(cmd, halt_on_failure=True,
                                               fatal_exit_code=3)

    def copy_mock_files(self, mock_target, files):
        """Copy files into the mock environment `mock_target`. `files` should
        be an iterable of 2-tuples: (src, dst)"""
        cmd_base = ['mock_mozilla', '-r', mock_target, '--copyin', '--unpriv']
        for src, dest in files:
            cmd = cmd_base + [src, dest]
            super(MockMixin, self).run_command(cmd, halt_on_failure=True,
                                               fatal_exit_code=3)
            super(MockMixin, self).run_command(
                ['mock_mozilla', '-r', mock_target, '--shell',
                 'chown -R mock_mozilla %s' % dest],
                halt_on_failure=True,
                fatal_exit_code=3)

    def get_mock_target(self):
        if self.config.get('disable_mock'):
            return None
        return self.default_mock_target or self.config.get('mock_target')

    def enable_mock(self):
        """Wrap self.run_command and self.get_output_from_command to run inside
        the mock environment given by self.config['mock_target']"""
        if not self.get_mock_target():
            return
        self.mock_enabled = True
        self.run_command = self.run_command_m
        self.get_output_from_command = self.get_output_from_command_m

    def disable_mock(self):
        """Restore self.run_command and self.get_output_from_command to their
        original versions. This is the opposite of self.enable_mock()"""
        if not self.get_mock_target():
            return
        self.mock_enabled = False
        self.run_command = super(MockMixin, self).run_command
        self.get_output_from_command = super(MockMixin, self).get_output_from_command

    def _do_mock_command(self, func, mock_target, command, cwd=None, env=None, **kwargs):
        """Internal helper for preparing commands to run under mock. Used by
        run_mock_command and get_mock_output_from_command."""
        cmd = ['mock_mozilla', '-r', mock_target, '-q']
        if cwd:
            cmd += ['--cwd', cwd]

        if not kwargs.get('privileged'):
            cmd += ['--unpriv']
        cmd += ['--shell']

        if not isinstance(command, basestring):
            command = subprocess.list2cmdline(command)

        # XXX - Hack - gets around AB_CD=%(locale)s type arguments
        command = command.replace("(", "\\(")
        command = command.replace(")", "\\)")

        if env:
            env_cmd = ['/usr/bin/env']
            for key, value in env.items():
                # $HOME won't work inside the mock chroot
                if key == 'HOME':
                    continue
                value = value.replace(";", "\\;")
                env_cmd += ['%s=%s' % (key, value)]
            cmd.append(subprocess.list2cmdline(env_cmd) + " " + command)
        else:
            cmd.append(command)
        return func(cmd, cwd=cwd, **kwargs)

    def run_mock_command(self, mock_target, command, cwd=None, env=None, **kwargs):
        """Same as ScriptMixin.run_command, except runs command inside mock
        environment `mock_target`."""
        return self._do_mock_command(
            super(MockMixin, self).run_command,
            mock_target, command, cwd, env, **kwargs)

    def get_mock_output_from_command(self, mock_target, command, cwd=None, env=None, **kwargs):
        """Same as ScriptMixin.get_output_from_command, except runs command
        inside mock environment `mock_target`."""
        return self._do_mock_command(
            super(MockMixin, self).get_output_from_command,
            mock_target, command, cwd, env, **kwargs)

    def reset_mock(self, mock_target=None):
        """rm mock lock and reset"""
        c = self.config
        if mock_target is None:
            if not c.get('mock_target'):
                self.fatal("Cound not determine: 'mock_target'")
            mock_target = c.get('mock_target')
        buildroot_lock_path = os.path.join(c.get('mock_mozilla_dir', ''),
                                           mock_target,
                                           'buildroot.lock')
        self.info("Removing buildroot lock at path if exists:O")
        self.info(buildroot_lock_path)
        if not os.path.exists(buildroot_lock_path):
            self.info(ERROR_MSGS['undetermined_buildroot_lock'])
        else:
            rm_lock_cmd = ['rm', '-f', buildroot_lock_path]
            super(MockMixin, self).run_command(rm_lock_cmd,
                                               halt_on_failure=True,
                                               fatal_exit_code=3)
        cmd = ['mock_mozilla', '-r', mock_target, '--orphanskill']
        return super(MockMixin, self).run_command(cmd, halt_on_failure=True,
                                                  fatal_exit_code=3)

    def setup_mock(self, mock_target=None, mock_packages=None, mock_files=None):
        """Initializes and installs packages, copies files into mock
        environment given by configuration in self.config.  The mock
        environment is given by self.config['mock_target'], the list of packges
        to install given by self.config['mock_packages'], and the list of files
        to copy in is self.config['mock_files']."""
        if self.done_mock_setup or self.config.get('disable_mock'):
            return

        c = self.config

        if mock_target is None:
            assert 'mock_target' in c
            t = c['mock_target']
        else:
            t = mock_target
        self.default_mock_target = t

        # Don't re-initialize mock if we're using the same packages as before
        # Put the cache inside the mock root so that if somebody else resets
        # the environment, it invalidates the cache
        mock_root = super(MockMixin, self).get_output_from_command(
            ['mock_mozilla', '-r', t, '--print-root-path']
        )
        package_hash_file = os.path.join(mock_root, "builds/package_list.hash")
        if os.path.exists(package_hash_file):
            old_packages_hash = self.read_from_file(package_hash_file)
            self.info("old package hash: %s" % old_packages_hash)
        else:
            self.info("no previous package list found")
            old_packages_hash = None

        if mock_packages is None:
            mock_packages = list(c.get('mock_packages'))

        package_list_hash = hashlib.new('sha1')
        if mock_packages:
            for p in sorted(mock_packages):
                package_list_hash.update(p)
        package_list_hash = package_list_hash.hexdigest()

        did_init = True
        # This simple hash comparison doesn't take into account depedency
        # changes. If you really care about dependencies, then they should be
        # explicitly listed in the package list.
        if old_packages_hash != package_list_hash:
            self.init_mock(t)
        else:
            self.info("Our list of packages hasn't changed; skipping re-initialization")
            did_init = False

        # Still try and install packages here since the package version may
        # have been updated on the server
        if mock_packages:
            self.install_mock_packages(t, mock_packages)

        # Save our list of packages
        self.write_to_file(package_hash_file,
                           package_list_hash)

        if mock_files is None:
            mock_files = list(c.get('mock_files'))
        if mock_files:
            if not did_init:
                # mock complains if you try and copy in files that already
                # exist, so we need to delete them here first
                self.info("Deleting old mock files")
                self.delete_mock_files(t, mock_files)
            self.copy_mock_files(t, mock_files)

        self.done_mock_setup = True

    def run_command_m(self, *args, **kwargs):
        """Executes self.run_mock_command if we have a mock target set,
        otherwise executes self.run_command."""
        mock_target = self.get_mock_target()
        if mock_target:
            self.setup_mock()
            return self.run_mock_command(mock_target, *args, **kwargs)
        else:
            return super(MockMixin, self).run_command(*args, **kwargs)

    def get_output_from_command_m(self, *args, **kwargs):
        """Executes self.get_mock_output_from_command if we have a mock target
        set, otherwise executes self.get_output_from_command."""
        mock_target = self.get_mock_target()
        if mock_target:
            self.setup_mock()
            return self.get_mock_output_from_command(mock_target, *args, **kwargs)
        else:
            return super(MockMixin, self).get_output_from_command(*args, **kwargs)
