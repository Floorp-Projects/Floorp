# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import subprocess

import mozfile
import mozpack.path as mozpath
from mozbuild.base import MozbuildObject
from mozfile import NamedTemporaryFile, TemporaryDirectory
from mozpack.files import FileFinder


class VendorPython(MozbuildObject):

    def vendor(self, packages=None):
        self.populate_logger()
        self.log_manager.enable_unstructured()

        vendor_dir = mozpath.join(
            self.topsrcdir, os.path.join('third_party', 'python'))

        packages = packages or []
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
                self._extract(tmp, vendor_dir)

        self.repository.add_remove_files(vendor_dir)

    def _extract(self, src, dest):
        """extract source distribution into vendor directory"""
        finder = FileFinder(src)
        for path, _ in finder.find('*'):
            # packages extract into package-version directory name and we strip the version
            tld = mozfile.extract(os.path.join(finder.base, path), dest)[0]
            target = os.path.join(dest, tld.rpartition('-')[0])
            mozfile.remove(target)  # remove existing version of vendored package
            mozfile.move(tld, target)
