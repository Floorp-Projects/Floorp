# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

r'''Make it easy to install and run [browsertime](https://github.com/sitespeedio/browsertime).

Browsertime is a harness for running performance tests, similar to
Mozilla's Raptor testing framework.  Browsertime is written in Node.js
and uses Selenium WebDriver to drive multiple browsers including
Chrome, Chrome for Android, Firefox, and (pending the resolution of
[Bug 1525126](https://bugzilla.mozilla.org/show_bug.cgi?id=1525126)
and similar tickets) Firefox for Android and GeckoView-based vehicles.

Right now a custom version of browsertime and the underlying
geckodriver binary are needed to support GeckoView-based vehicles;
this module accommodates those in-progress custom versions.

To get started, run
```
./mach browsertime --setup [--clobber]
```
This will populate `tools/browsertime/node_modules`.

To invoke browsertime, run
```
./mach browsertime [ARGS]
```
All arguments are passed through to browsertime.
'''

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import sys

from mach.decorators import CommandArgument, CommandProvider, Command
from mozbuild.base import MachCommandBase
import mozpack.path as mozpath


BROWSERTIME_ROOT = os.path.dirname(__file__)


def host_platform():
    is_64bits = sys.maxsize > 2**32

    if sys.platform.startswith('win'):
        if is_64bits:
            return 'win64'
    elif sys.platform.startswith('linux'):
        if is_64bits:
            return 'linux64'
    elif sys.platform.startswith('darwin'):
        return 'darwin'

    raise ValueError('sys.platform is not yet supported: {}'.format(sys.platform))


# Map from `host_platform()` to a `fetch`-like syntax.
host_fetches = {
    'darwin': {
        'ffmpeg': {
            'type': 'static-url',
            'url': 'https://ffmpeg.zeranoe.com/builds/macos64/static/ffmpeg-4.1.1-macos64-static.zip',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ffmpeg-4.1.1-macos64-static',
        },
        'ImageMagick': {
            'type': 'static-url',
            # It's sad that the macOS URLs don't include version numbers.  If
            # ImageMagick is released frequently, we'll need to be more
            # accommodating of multiple versions here.
            'url': 'https://imagemagick.org/download/binaries/ImageMagick-x86_64-apple-darwin17.7.0.tar.gz',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ImageMagick-7.0.8',
        },
    },
    'linux64': {
        'ffmpeg': {
            'type': 'static-url',
            'url': 'https://www.johnvansickle.com/ffmpeg/old-releases/ffmpeg-4.0.3-64bit-static.tar.xz',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ffmpeg-4.0.3-64bit-static',
        },
        # TODO: install a static ImageMagick.  These binaries are not
        # statically linked, so they will (mostly) fail at runtime due to
        # missing dependencies.  For now we require folks to install
        # ImageMagick globally with their package manager of choice.
        'ImageMagick': {
            'type': 'static-url',
            'url': 'https://imagemagick.org/download/binaries/ImageMagick-x86_64-pc-linux-gnu.tar.gz',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ImageMagick-6.9.2',
        },
    },
    'win64': {
        'ffmpeg': {
            'type': 'static-url',
            'url': 'https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-4.1.1-win64-static.zip',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ffmpeg-4.1.1-win64-static',
        },
        'ImageMagick': {
            'type': 'static-url',
            # 'url': 'https://imagemagick.org/download/binaries/ImageMagick-7.0.8-39-portable-Q16-x64.zip',  # noqa
            # imagemagick.org doesn't keep old versions; the mirror below does.
            'url': 'https://ftp.icm.edu.pl/packages/ImageMagick/binaries/ImageMagick-7.0.8-39-portable-Q16-x64.zip',  # noqa
            # An extension to `fetch` syntax.
            'path': 'ImageMagick-7.0.8',
        },
    },
}


@CommandProvider
class MachBrowsertime(MachCommandBase):
    @property
    def artifact_cache_path(self):
        r'''Downloaded artifacts will be kept here.'''
        # The convention is $MOZBUILD_STATE_PATH/cache/$FEATURE.
        return mozpath.join(self._mach_context.state_dir, 'cache', 'browsertime')

    @property
    def state_path(self):
        r'''Unpacked artifacts will be kept here.'''
        # The convention is $MOZBUILD_STATE_PATH/$FEATURE.
        return mozpath.join(self._mach_context.state_dir, 'browsertime')

    def setup(self, should_clobber=False):
        r'''Install browsertime and visualmetrics.py requirements.'''

        from mozbuild.action.tooltool import unpack_file
        from mozbuild.artifact_cache import ArtifactCache
        sys.path.append(mozpath.join(self.topsrcdir, 'tools', 'lint', 'eslint'))
        import setup_helper

        # Download the visualmetrics.py requirements.
        artifact_cache = ArtifactCache(self.artifact_cache_path,
                                       log=self.log, skip_cache=False)

        fetches = host_fetches[host_platform()]
        for tool, fetch in sorted(fetches.items()):
            archive = artifact_cache.fetch(fetch['url'])
            # TODO: assert type, verify sha256 (and size?).

            if fetch.get('unpack', True):
                cwd = os.getcwd()
                try:
                    os.chdir(self.state_path)
                    self.log(
                        logging.INFO,
                        'browsertime',
                        {'path': archive},
                        'Unpacking temporary location {path}')
                    unpack_file(archive)
                finally:
                    os.chdir(cwd)

        # Install the browsertime Node.js requirements.
        if not setup_helper.check_node_executables_valid():
            return 1

        self.log(
            logging.INFO,
            'browsertime',
            {'package_json': mozpath.join(BROWSERTIME_ROOT, 'package.json')},
            'Installing browsertime node module from {package_json}')
        status = setup_helper.package_setup(
            BROWSERTIME_ROOT,
            'browsertime',
            should_clobber=should_clobber)

        if status:
            return status

        return self.check()

    @property
    def node_path(self):
        from mozbuild.nodeutil import find_node_executable
        node, _ = find_node_executable()

        return os.path.abspath(node)

    def node(self, args):
        r'''Invoke node (interactively) with the given arguments.'''
        return self.run_process(
            [self.node_path] + args,
            append_env=self.append_env(),
            pass_thru=True,  # Allow user to run Node interactively.
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

    @property
    def package_path(self):
        r'''The path to the `browsertime` directory.

        Override the default with the `BROWSERTIME` environment variable.'''
        override = os.environ.get('BROWSERTIME', None)
        if override:
            return override

        return mozpath.join(BROWSERTIME_ROOT, 'node_modules', 'browsertime')

    @property
    def browsertime_path(self):
        '''The path to the `browsertime.js` script.'''
        # On Windows, invoking `node_modules/.bin/browsertime{.cmd}`
        # doesn't work when invoked as an argument to our specific
        # binary.  Since we want our version of node, invoke the
        # actual script directly.
        return mozpath.join(
            self.package_path,
            'bin',
            'browsertime.js')

    @property
    def visualmetrics_path(self):
        '''The path to the `visualmetrics.py` script.'''
        return mozpath.join(
            self.package_path,
            'vendor',
            'visualmetrics.py')

    def append_env(self, append_path=True):
        fetches = host_fetches[host_platform()]

        # Ensure that bare `ffmpeg` and ImageMagick commands
        # {`convert`,`compare`,`mogrify`} are found.  The `visualmetrics.py`
        # script doesn't take these as configuration, so we do this (for now).
        # We should update the script itself to accept this configuration.
        path = os.environ.get('PATH', '').split(os.pathsep) if append_path else []
        path_to_ffmpeg = mozpath.join(
            self.state_path,
            fetches['ffmpeg']['path'])

        path_to_imagemagick = mozpath.join(
            self.state_path,
            fetches['ImageMagick']['path'])

        path = [
            path_to_ffmpeg if host_platform().startswith('linux') else mozpath.join(path_to_ffmpeg, 'bin'),  # noqa
            self.state_path if host_platform().startswith('win') else mozpath.join(path_to_imagemagick, 'bin'),  # noqa
        ] + path

        # Ensure that bare `node` and `npm` in scripts, including post-install
        # scripts, finds the binary we're invoking with.  Without this, it's
        # easy for compiled extensions to get mismatched versions of the Node.js
        # extension API.
        node_dir = os.path.dirname(self.node_path)
        path = [node_dir] + path

        # Ensure that `/usr/bin/env python` in `visualmetrics.py` finds our
        # virtualenv Python.
        path = [os.path.dirname(self.virtualenv_manager.python_path)] + path

        return {
            # See https://imagemagick.org/script/download.php.  Harmless on other platforms.
            'LD_LIBRARY_PATH': mozpath.join(path_to_imagemagick, 'lib'),
            'DYLD_LIBRARY_PATH': mozpath.join(path_to_imagemagick, 'lib'),
            'MAGICK_HOME': path_to_imagemagick,
            'PATH': os.pathsep.join(path),
        }

    def _activate_virtualenv(self, *args, **kwargs):
        MachCommandBase._activate_virtualenv(self, *args, **kwargs)

        try:
            self.virtualenv_manager.install_pip_package('Pillow==6.0.0')
        except Exception:
            print('Could not install Pillow from pip.')
            return 1

        try:
            self.virtualenv_manager.install_pip_package('pyssim==0.4')
        except Exception:
            print('Could not install pyssim from pip.')
            return 1

    def check(self):
        r'''Run `visualmetrics.py --check`.'''
        self._activate_virtualenv()

        args = ['--check']
        status = self.run_process(
            [self.virtualenv_manager.python_path, self.visualmetrics_path] + args,
            # For --check, don't allow user's path to interfere with
            # path testing except on Linux, where ImageMagick needs to
            # be installed manually.
            append_env=self.append_env(append_path=host_platform().startswith('linux')),
            pass_thru=True,
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))

        sys.stdout.flush()
        sys.stderr.flush()

        if status:
            return status

        # Avoid logging the command (and, on Windows, the environment).
        self.log_manager.terminal_handler.setLevel(logging.CRITICAL)
        print('browsertime version:', end=' ')

        sys.stdout.flush()
        sys.stderr.flush()

        return self.node([self.browsertime_path] + ['--version'])

    @Command('browsertime', category='testing',
             description='Run [browsertime](https://github.com/sitespeedio/browsertime) '
                         'performance tests.')
    @CommandArgument('--verbose', action='store_true',
                     help='Verbose output for what commands the build is running.')
    @CommandArgument('--setup', default=False, action='store_true')
    @CommandArgument('--clobber', default=False, action='store_true')
    @CommandArgument('--skip-cache', action='store_true',
                     help='Skip all local caches to force re-fetching remote artifacts.',
                     default=False)
    @CommandArgument('--check', default=False, action='store_true')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def browsertime(self, args, verbose=False,
                    setup=False, clobber=False, skip_cache=False,
                    check=False):
        self._set_log_level(True)

        if setup:
            return self.setup(should_clobber=clobber)

        if check:
            return self.check()

        self._activate_virtualenv()

        return self.node([self.browsertime_path] + args)

    @Command('visualmetrics', category='testing',
             description='Run visualmetrics.py')
    @CommandArgument('video')
    @CommandArgument('args', nargs=argparse.REMAINDER)
    def visualmetrics(self, video, args):
        self._set_log_level(True)
        self._activate_virtualenv()

        # Turn '/path/to/video/1.mp4' into '/path/to/video' and '1'.
        d, base = os.path.split(video)
        index, _ = os.path.splitext(base)

        # TODO: write a '--logfile' as well.
        args = ['--dir',  # Images are written to `/path/to/video/images` (following browsertime).
                mozpath.join(d, 'images', index),
                '--video',
                video,
                '--orange',
                '--perceptual',
                '--force',
                '--renderignore',
                '5',
                '--json',
                '--viewport',
                '-q',
                '75',
                '-vvvv']
        return self.run_process(
            [self.visualmetrics_path] + args,
            append_env=self.append_env(),
            pass_thru=True,
            ensure_exit_code=False,  # Don't throw on non-zero exit code.
            cwd=mozpath.join(self.topsrcdir))
