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

import mozhttpd

from mozbuild.base import MachCommandBase


@CommandProvider
class Documentation(MachCommandBase):
    """Helps manage in-tree documentation."""

    @Command('doc', category='devenv',
        description='Generate and display documentation from the tree.')
    @CommandArgument('what', nargs='*', metavar='DIRECTORY [, DIRECTORY]',
        help='Path(s) to documentation to build and display.')
    @CommandArgument('--format', default='html',
        help='Documentation format to write.')
    @CommandArgument('--outdir', default=None, metavar='DESTINATION',
        help='Where to write output.')
    @CommandArgument('--no-open', dest='auto_open', default=True, action='store_false',
        help="Don't automatically open HTML docs in a browser.")
    @CommandArgument('--http', const=':6666', metavar='ADDRESS', nargs='?',
        help='Serve documentation on an HTTP server, e.g. ":6666".')
    def build_docs(self, what=None, format=None, outdir=None, auto_open=True, http=None):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('sphinx_rtd_theme==0.1.6')

        import sphinx
        import webbrowser

        if not outdir:
            outdir = os.path.join(self.topobjdir, 'docs')
        if not what:
            what = [os.path.join(self.topsrcdir, 'tools')]
        outdir = os.path.join(outdir, format)

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
            savedir = os.path.join(outdir, project)

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

            index_path = os.path.join(savedir, 'index.html')
            if not http and auto_open and os.path.isfile(index_path):
                webbrowser.open(index_path)

        if generated:
            print('\nGenerated documentation:\n%s\n' % '\n'.join(generated))

        if failed:
            failed = ['%s: %s' % (f[0], f[1]) for f in failed]
            return die('failed to generate documentation:\n%s' % '\n'.join(failed))

        if http is not None:
            host, port = http.split(':', 1)
            addr = (host, int(port))
            if len(addr) != 2:
                return die('invalid address: %s' % http)

            httpd = mozhttpd.MozHttpd(host=addr[0], port=addr[1], docroot=outdir)
            print('listening on %s:%d' % addr)
            httpd.start(block=True)

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

    @Command('doc-upload', category='devenv',
        description='Generate and upload documentation from the tree.')
    @CommandArgument('--bucket', required=True,
        help='Target S3 bucket.')
    @CommandArgument('--region', required=True,
        help='Region containing target S3 bucket.')
    @CommandArgument('what', nargs='*', metavar='DIRECTORY [, DIRECTORY]',
        help='Path(s) to documentation to build and upload.')
    def upload_docs(self, bucket, region, what=None):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('boto3==1.4.4')

        outdir = os.path.join(self.topobjdir, 'docs')
        self.build_docs(what=what, outdir=outdir, format='html')

        self.s3_upload(os.path.join(outdir, 'html', 'Mozilla_Source_Tree_Docs'), bucket, region)

    def s3_upload(self, root, bucket, region):
        """Upload the contents of outdir recursively to S3"""
        import boto3
        import mimetypes
        import requests

        # Get the credentials from the TC secrets service.  Note that these are
        # only available to level-3 pushes.
        if 'TASK_ID' in os.environ:
            print("Using AWS credentials from the secrets service")
            session = requests.Session()
            secrets_url = 'http://taskcluster/secrets/repo:hg.mozilla.org/mozilla-central/gecko-docs-upload'
            res = session.get(secrets_url)
            res.raise_for_status()
            secret = res.json()
            session = boto3.session.Session(
                aws_access_key_id=secret['AWS_ACCESS_KEY_ID'],
                aws_secret_access_key=secret['AWS_SECRET_ACCESS_KEY'],
                region_name=region)
        else:
            print("Trying to use your AWS credentials..")
            session = boto3.session.Session(region_name=region)
        s3 = session.client('s3')

        try:
            old_cwd = os.getcwd()
            os.chdir(root)

            for dir, dirs, filenames in os.walk('.'):
                if dir == '.':
                    # ignore a few things things in the root directory
                    bad = [d for d in dirs if d.startswith('.') or d in ('_venv', '_staging')]
                    for b in bad:
                        dirs.remove(b)
                for filename in filenames:
                    pathname = os.path.join(dir, filename)[2:]  # strip '.''
                    content_type, content_encoding = mimetypes.guess_type(pathname)
                    extra_args = {}
                    if content_type:
                        extra_args['ContentType'] = content_type
                    if content_encoding:
                        extra_args['ContentEncoding'] = content_encoding
                    print('uploading', pathname)
                    s3.upload_file(pathname, bucket, pathname, ExtraArgs=extra_args)
        finally:
            os.chdir(old_cwd)

def die(msg, exit_code=1):
    msg = '%s: %s' % (sys.argv[0], msg)
    print(msg, file=sys.stderr)
    return exit_code
