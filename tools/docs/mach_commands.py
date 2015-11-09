# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase


@CommandProvider
class Documentation(MachCommandBase):
    """Helps manage in-tree documentation."""

    @Command('doc', category='devenv',
        description='Generate and display documentation from the tree.')
    @CommandArgument('what', default=None, nargs='*',
        help='Path(s) to documentation to build and display.')
    @CommandArgument('--format', default='html',
        help='Documentation format to write.')
    @CommandArgument('--outdir', default=None,
        help='Where to write output.')
    @CommandArgument('--no-open', dest='auto_open', default=True, action='store_false',
        help='Don\'t automatically open HTML docs in a browser.')
    def build_docs(self, what=None, format=None, outdir=None, auto_open=True):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('sphinx_rtd_theme==0.1.6')

        import sphinx
        import webbrowser

        if not outdir:
            outdir = os.path.join(self.topobjdir, 'docs')

        if not what:
            what = [os.path.join(self.topsrcdir, 'tools')]

        generated = []
        failed = []
        for path in what:
            path = os.path.normpath(os.path.abspath(path))
            docdir = self._find_doc_dir(path)

            if not docdir:
                failed.append((path, 'could not find docs at this location'))
                continue

            # find project name to use as a namespace within `outdir`
            project = self._find_project_name(docdir)
            savedir = os.path.join(outdir, format, project)

            args = [
                'sphinx',
                '-b', format,
                docdir,
                savedir,
            ]
            result = sphinx.build_main(args)
            if result != 0:
                failed.append((path, 'sphinx return code %d' % result))
            else:
                generated.append(savedir)

            # attempt to open html docs in a browser
            index_path = os.path.join(savedir, 'index.html')
            if auto_open and os.path.isfile(index_path):
                webbrowser.open(index_path)

        if generated:
            print("\nGenerated documentation:\n%s\n" % '\n'.join(generated))

        if failed:
            failed = ['%s - %s' % (f[0], f[1]) for f in failed]
            print("ERROR failed to generate documentation:\n%s" % '\n'.join(failed))
            return 1

    def _find_project_name(self, path):
        import imp
        path = os.path.join(path, 'conf.py')
        with open(path, 'r') as fh:
            conf = imp.load_module('doc_conf', fh, path,
                                   ('.py', 'r', imp.PY_SOURCE))

        return conf.project.replace(' ', '_')

    def _find_doc_dir(self, path):
        search_dirs = ('doc', 'docs')
        for d in search_dirs:
            p = os.path.join(path, d)
            if os.path.isfile(os.path.join(p, 'conf.py')):
                return p
