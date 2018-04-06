# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys

from mach.decorators import (
    Command,
    CommandArgument,
    CommandProvider,
)

import which
import mozhttpd

from mozbuild.base import MachCommandBase

here = os.path.abspath(os.path.dirname(__file__))


@CommandProvider
class Documentation(MachCommandBase):
    """Helps manage in-tree documentation."""

    @Command('doc', category='devenv',
             description='Generate and display documentation from the tree.')
    @CommandArgument('path', default=None, metavar='DIRECTORY', nargs='?',
                     help='Path to documentation to build and display.')
    @CommandArgument('--format', default='html',
                     help='Documentation format to write.')
    @CommandArgument('--outdir', default=None, metavar='DESTINATION',
                     help='Where to write output.')
    @CommandArgument('--archive', action='store_true',
                     help='Write a gzipped tarball of generated docs')
    @CommandArgument('--no-open', dest='auto_open', default=True,
                     action='store_false',
                     help="Don't automatically open HTML docs in a browser.")
    @CommandArgument('--http', const=':6666', metavar='ADDRESS', nargs='?',
                     help='Serve documentation on an HTTP server, '
                          'e.g. ":6666".')
    @CommandArgument('--upload', action='store_true',
                     help='Upload generated files to S3')
    def build_docs(self, path=None, format=None, outdir=None, auto_open=True,
                   http=None, archive=False, upload=False):
        try:
            which.which('jsdoc')
        except which.WhichError:
            return die('jsdoc not found - please install from npm.')

        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_requirements(
            os.path.join(here, 'requirements.txt'), quiet=True)

        import sphinx
        import webbrowser
        import moztreedocs

        outdir = outdir or os.path.join(self.topobjdir, 'docs')
        format_outdir = os.path.join(outdir, format)

        path = path or os.path.join(self.topsrcdir, 'tools')
        path = os.path.normpath(os.path.abspath(path))

        docdir = self._find_doc_dir(path)
        if not docdir:
            return die('failed to generate documentation:\n'
                       '%s: could not find docs at this location' % path)

        props = self._project_properties(docdir)
        savedir = os.path.join(format_outdir, props['project'])

        args = [
            'sphinx',
            '-b', format,
            docdir,
            savedir,
        ]
        result = sphinx.build_main(args)
        if result != 0:
            return die('failed to generate documentation:\n'
                       '%s: sphinx return code %d' % (path, result))
        else:
            print('\nGenerated documentation:\n%s' % savedir)

        if archive:
            archive_path = os.path.join(outdir,
                                        '%s.tar.gz' % props['project'])
            moztreedocs.create_tarball(archive_path, savedir)
            print('Archived to %s' % archive_path)

        if upload:
            self._s3_upload(savedir, props['project'], props['version'])

        index_path = os.path.join(savedir, 'index.html')
        if not http and auto_open and os.path.isfile(index_path):
            webbrowser.open(index_path)

        if http is not None:
            host, port = http.split(':', 1)
            addr = (host, int(port))
            if len(addr) != 2:
                return die('invalid address: %s' % http)

            httpd = mozhttpd.MozHttpd(host=addr[0], port=addr[1],
                                      docroot=format_outdir)
            print('listening on %s:%d' % addr)
            httpd.start(block=True)

    def _project_properties(self, path):
        import imp
        path = os.path.join(path, 'conf.py')
        with open(path, 'r') as fh:
            conf = imp.load_module('doc_conf', fh, path,
                                   ('.py', 'r', imp.PY_SOURCE))

        # Prefer the Mozilla project name, falling back to Sphinx's
        # default variable if it isn't defined.
        project = getattr(conf, 'moz_project_name', None)
        if not project:
            project = conf.project.replace(' ', '_')

        return {
            'project': project,
            'version': getattr(conf, 'version', None)
        }

    def _find_doc_dir(self, path):
        search_dirs = ('doc', 'docs')
        for d in search_dirs:
            p = os.path.join(path, d)
            if os.path.isfile(os.path.join(p, 'conf.py')):
                return p

    def _s3_upload(self, root, project, version=None):
        self.virtualenv_manager.install_pip_package('boto3==1.4.4')

        from moztreedocs import distribution_files
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
