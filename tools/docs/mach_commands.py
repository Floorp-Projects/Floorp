# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
from functools import partial

from mach.decorators import (
    Command,
    CommandArgument,
    CommandProvider,
)

import which
from mozbuild.base import MachCommandBase

here = os.path.abspath(os.path.dirname(__file__))


@CommandProvider
class Documentation(MachCommandBase):
    """Helps manage in-tree documentation."""

    def __init__(self, context):
        super(Documentation, self).__init__(context)

        self._manager = None
        self._project = None
        self._version = None

    @Command('doc', category='devenv',
             description='Generate and serve documentation from the tree.')
    @CommandArgument('path', default=None, metavar='DIRECTORY', nargs='?',
                     help='Path to documentation to build and display.')
    @CommandArgument('--format', default='html', dest='fmt',
                     help='Documentation format to write.')
    @CommandArgument('--outdir', default=None, metavar='DESTINATION',
                     help='Where to write output.')
    @CommandArgument('--archive', action='store_true',
                     help='Write a gzipped tarball of generated docs.')
    @CommandArgument('--no-open', dest='auto_open', default=True,
                     action='store_false',
                     help="Don't automatically open HTML docs in a browser.")
    @CommandArgument('--no-serve', dest='serve', default=True, action='store_false',
                     help="Don't serve the generated docs after building.")
    @CommandArgument('--http', default='localhost:5500', metavar='ADDRESS',
                     help='Serve documentation on the specified host and port, '
                          'default "localhost:5500".')
    @CommandArgument('--upload', action='store_true',
                     help='Upload generated files to S3.')
    def build_docs(self, path=None, fmt='html', outdir=None, auto_open=True,
                   serve=True, http=None, archive=False, upload=False):
        try:
            which.which('jsdoc')
        except which.WhichError:
            return die('jsdoc not found - please install from npm.')

        self.activate_pipenv(os.path.join(here, 'Pipfile'))

        import webbrowser
        from livereload import Server
        from moztreedocs.package import create_tarball

        outdir = outdir or os.path.join(self.topobjdir, 'docs')
        savedir = os.path.join(outdir, fmt)

        path = path or os.path.join(self.topsrcdir, 'tools')
        path = os.path.normpath(os.path.abspath(path))

        docdir = self._find_doc_dir(path)
        if not docdir:
            return die('failed to generate documentation:\n'
                       '%s: could not find docs at this location' % path)

        result = self._run_sphinx(docdir, savedir, fmt=fmt)
        if result != 0:
            return die('failed to generate documentation:\n'
                       '%s: sphinx return code %d' % (path, result))
        else:
            print('\nGenerated documentation:\n%s' % savedir)

        if archive:
            archive_path = os.path.join(outdir, '%s.tar.gz' % self.project)
            create_tarball(archive_path, savedir)
            print('Archived to %s' % archive_path)

        if upload:
            self._s3_upload(savedir, self.project, self.version)

        if not serve:
            index_path = os.path.join(savedir, 'index.html')
            if auto_open and os.path.isfile(index_path):
                webbrowser.open(index_path)
            return

        # Create livereload server. Any files modified in the specified docdir
        # will cause a re-build and refresh of the browser (if open).
        try:
            host, port = http.split(':', 1)
            port = int(port)
        except ValueError:
            return die('invalid address: %s' % http)

        server = Server()

        sphinx_trees = self.manager.trees or {savedir: docdir}
        for dest, src in sphinx_trees.items():
            run_sphinx = partial(self._run_sphinx, src, savedir, fmt=fmt)
            server.watch(src, run_sphinx)
        server.serve(host=host, port=port, root=savedir,
                     open_url_delay=0.1 if auto_open else None)

    def _run_sphinx(self, docdir, savedir, config=None, fmt='html'):
        import sphinx
        config = config or self.manager.conf_py_path
        args = [
            'sphinx',
            '-b', fmt,
            '-c', os.path.dirname(config),
            docdir,
            savedir,
        ]
        return sphinx.build_main(args)

    @property
    def manager(self):
        if not self._manager:
            from moztreedocs import manager
            self._manager = manager
        return self._manager

    def _read_project_properties(self):
        import imp
        path = os.path.normpath(self.manager.conf_py_path)
        with open(path, 'r') as fh:
            conf = imp.load_module('doc_conf', fh, path,
                                   ('.py', 'r', imp.PY_SOURCE))

        # Prefer the Mozilla project name, falling back to Sphinx's
        # default variable if it isn't defined.
        project = getattr(conf, 'moz_project_name', None)
        if not project:
            project = conf.project.replace(' ', '_')

        self._project = project
        self._version = getattr(conf, 'version', None)

    @property
    def project(self):
        if not self._project:
            self._read_project_properties()
        return self._project

    @property
    def version(self):
        if not self._version:
            self._read_project_properties()
        return self._version

    def _find_doc_dir(self, path):
        if os.path.isfile(path):
            return

        valid_doc_dirs = ('doc', 'docs')
        if os.path.basename(path) in valid_doc_dirs:
            return path

        for d in valid_doc_dirs:
            p = os.path.join(path, d)
            if os.path.isdir(p):
                return p

    def _s3_upload(self, root, project, version=None):
        from moztreedocs.package import distribution_files
        from moztreedocs.upload import s3_upload

        # Files are uploaded to multiple locations:
        #
        # <project>/latest
        # <project>/<version>
        #
        # This allows multiple projects and versions to be stored in the
        # S3 bucket.

        files = list(distribution_files(root))

        s3_upload(files, key_prefix='%s/latest' % project)
        if version:
            s3_upload(files, key_prefix='%s/%s' % (project, version))

        # Until we redirect / to main/latest, upload the main docs
        # to the root.
        if project == 'main':
            s3_upload(files)


def die(msg, exit_code=1):
    msg = '%s: %s' % (sys.argv[0], msg)
    print(msg, file=sys.stderr)
    return exit_code
