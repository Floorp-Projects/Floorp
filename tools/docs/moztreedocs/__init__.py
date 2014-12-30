# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mozbuild.frontend.reader import BuildReader
from mozpack.copier import FileCopier
from mozpack.files import FileFinder
from mozpack.manifests import InstallManifest

import sphinx
import sphinx.apidoc


class SphinxManager(object):
    """Manages the generation of Sphinx documentation for the tree."""

    def __init__(self, topsrcdir, main_path, output_dir):
        self._topsrcdir = topsrcdir
        self._output_dir = output_dir
        self._docs_dir = os.path.join(output_dir, '_staging')
        self._conf_py_path = os.path.join(main_path, 'conf.py')
        self._index_path = os.path.join(main_path, 'index.rst')
        self._trees = {}
        self._python_package_dirs = set()

    def read_build_config(self):
        """Read the active build config and add docs to this instance."""

        # Reading the Sphinx variables doesn't require a full build context.
        # Only define the parts we need.
        class fakeconfig(object):
            def __init__(self, topsrcdir):
                self.topsrcdir = topsrcdir

        config = fakeconfig(self._topsrcdir)
        reader = BuildReader(config)

        for path, name, key, value in reader.find_sphinx_variables():
            reldir = os.path.dirname(path)

            if name == 'SPHINX_TREES':
                assert key
                self.add_tree(os.path.join(reldir, value),
                    os.path.join(reldir, key))

            if name == 'SPHINX_PYTHON_PACKAGE_DIRS':
                self.add_python_package_dir(os.path.join(reldir, value))

    def add_tree(self, source_dir, dest_dir):
        """Add a directory from where docs should be sourced."""
        if dest_dir in self._trees:
            raise Exception('%s has already been registered as a destination.'
                % dest_dir)

        self._trees[dest_dir] = source_dir

    def add_python_package_dir(self, source_dir):
        """Add a directory containing Python packages.

        Added directories will have Python API docs generated automatically.
        """
        self._python_package_dirs.add(source_dir)

    def generate_docs(self, app):
        """Generate/stage documentation."""
        app.info('Reading Sphinx metadata from build configuration')
        self.read_build_config()
        app.info('Staging static documentation')
        self._synchronize_docs()
        app.info('Generating Python API documentation')
        self._generate_python_api_docs()

    def _generate_python_api_docs(self):
        """Generate Python API doc files."""
        out_dir = os.path.join(self._docs_dir, 'python')
        base_args = ['sphinx', '--no-toc', '-o', out_dir]

        for p in sorted(self._python_package_dirs):
            full = os.path.join(self._topsrcdir, p)

            finder = FileFinder(full, find_executables=False)
            dirs = {os.path.dirname(f[0]) for f in finder.find('**')}

            excludes = {d for d in dirs if d.endswith('test')}

            args = list(base_args)
            args.append(full)
            args.extend(excludes)

            sphinx.apidoc.main(args)

    def _synchronize_docs(self):
        m = InstallManifest()

        m.add_symlink(self._conf_py_path, 'conf.py')

        for dest, source in sorted(self._trees.items()):
            source_dir = os.path.join(self._topsrcdir, source)
            for root, dirs, files in os.walk(source_dir):
                for f in files:
                    source_path = os.path.join(root, f)
                    rel_source = source_path[len(source_dir) + 1:]

                    m.add_symlink(source_path, os.path.join(dest, rel_source))

        copier = FileCopier()
        m.populate_registry(copier)
        copier.copy(self._docs_dir)

        with open(self._index_path, 'rb') as fh:
            data = fh.read()

        indexes = ['%s/index' % p for p in sorted(self._trees.keys())]
        indexes = '\n   '.join(indexes)

        packages = [os.path.basename(p) for p in self._python_package_dirs]
        packages = ['python/%s' % p for p in packages]
        packages = '\n   '.join(sorted(packages))
        data = data.format(indexes=indexes, python_packages=packages)

        with open(os.path.join(self._docs_dir, 'index.rst'), 'wb') as fh:
            fh.write(data)
