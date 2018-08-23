# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import shutil
import subprocess

import mozfile
import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozfile import NamedTemporaryFile, TemporaryDirectory
from mozpack.files import FileFinder


class VendorPython(MozbuildObject):

    def vendor(self, packages=None, with_windows_wheel=False):
        self.populate_logger()
        self.log_manager.enable_unstructured()

        vendor_dir = mozpath.join(
            self.topsrcdir, os.path.join('third_party', 'python'))

        packages = packages or []
        if with_windows_wheel and len(packages) != 1:
            raise Exception('--with-windows-wheel is only supported for a single package!')
        pipenv = self.ensure_pipenv()

        for package in packages:
            if not all(package.partition('==')):
                raise Exception('Package {} must be in the format name==version'.format(package))

        for package in packages:
            subprocess.check_call(
                [pipenv, 'install', package],
                cwd=self.topsrcdir)

        with NamedTemporaryFile('w') as requirements:
            # determine the dependency graph and generate requirements.txt
            subprocess.check_call(
                [pipenv, 'lock', '--requirements'],
                cwd=self.topsrcdir,
                stdout=requirements)

            with TemporaryDirectory() as tmp:
                # use requirements.txt to download archived source distributions of all packages
                self.virtualenv_manager._run_pip([
                    'download',
                    '-r', requirements.name,
                    '--no-deps',
                    '--dest', tmp,
                    '--no-binary', ':all:',
                    '--disable-pip-version-check'])
                if with_windows_wheel:
                    # This is hardcoded to CPython 2.7 for win64, which is good
                    # enough for what we need currently. If we need psutil for Python 3
                    # in the future that coudl be added here as well.
                    self.virtualenv_manager._run_pip([
                        'download',
                        '--dest', tmp,
                        '--no-deps',
                        '--only-binary', ':all:',
                        '--platform', 'win_amd64',
                        '--implementation', 'cp',
                        '--python-version', '27',
                        '--abi', 'none',
                        '--disable-pip-version-check',
                        packages[0]])
                self._extract(tmp, vendor_dir)

        self.repository.add_remove_files(vendor_dir)

    def _extract(self, src, dest):
        """extract source distribution into vendor directory"""
        finder = FileFinder(src)
        for path, _ in finder.find('*'):
            base, ext = os.path.splitext(path)
            if ext == '.whl':
                # Wheels would extract into a directory with the name of the package, but
                # we want the platform signifiers, minus the version number.
                # Wheel filenames look like:
                # {distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}
                bits = base.split('-')

                # Remove the version number.
                bits.pop(1)
                target = os.path.join(dest, '-'.join(bits))
                mozfile.remove(target)  # remove existing version of vendored package
                os.mkdir(target)
                mozfile.extract(os.path.join(finder.base, path), target)
            else:
                # packages extract into package-version directory name and we strip the version
                tld = mozfile.extract(os.path.join(finder.base, path), dest)[0]
                target = os.path.join(dest, tld.rpartition('-')[0])
                mozfile.remove(target)  # remove existing version of vendored package
                mozfile.move(tld, target)
            # If any files inside the vendored package were symlinks, turn them into normal files
            # because hg.mozilla.org forbids symlinks in the repository.
            link_finder = FileFinder(target)
            for _, f in link_finder.find('**'):
                if os.path.islink(f.path):
                    link_target = os.path.realpath(f.path)
                    os.unlink(f.path)
                    shutil.copyfile(link_target, f.path)
