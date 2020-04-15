# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
from mozbuild.base import (
    MozbuildObject,
)
import mozfile
import mozpack.path as mozpath
import os
import requests
import re
import sys
import tarfile
from urllib.parse import urlparse


class VendorDav1d(MozbuildObject):
    def upstream_snapshot(self, revision):
        '''Construct a url for a tarball snapshot of the given revision.'''
        if 'code.videolan.org' in self.repo_url:
            return mozpath.join(self.repo_url, '-', 'archive', revision + '.tar.gz')
        else:
            raise ValueError('Unknown git host, no snapshot lookup method')

    def upstream_commit(self, revision):
        '''Convert a revision to a git commit and timestamp.

        Ask the upstream repo to convert the requested revision to
        a git commit id and timestamp, so we can be precise in
        what we're vendoring.'''
        if 'code.videolan.org' in self.repo_url:
            return self.upstream_gitlab_commit(revision)
        else:
            raise ValueError('Unknown git host, no commit lookup method')

    def upstream_validate(self, url):
        '''Validate repository urls to make sure we can handle them.'''
        host = urlparse(url).netloc
        valid_domains = ('code.videolan.org')
        if not any(filter(lambda domain: domain in host, valid_domains)):
            self.log(logging.ERROR, 'upstream_url', {},
                     '''Unsupported git host %s; cannot fetch snapshots.

Please set a repository url with --repo on either googlesource or github.''' % host)
            sys.exit(1)

    def upstream_gitlab_commit(self, revision):
        '''Query the github api for a git commit id and timestamp.'''
        gitlab_api = 'https://code.videolan.org/api/v4/projects/videolan%2Fdav1d/repository/commits'  # noqa
        url = mozpath.join(gitlab_api, revision)
        self.log(logging.INFO, 'fetch', {'url': url},
                 'Fetching commit id from {url}')
        req = requests.get(url)
        req.raise_for_status()
        info = req.json()
        return (info['id'], info['committed_date'])

    def fetch_and_unpack(self, revision, target):
        '''Fetch and unpack upstream source'''
        url = self.upstream_snapshot(revision)
        self.log(logging.INFO, 'fetch', {'url': url}, 'Fetching {url}')
        prefix = 'dav1d-' + revision
        filename = prefix + '.tar.gz'
        with open(filename, 'wb') as f:
            req = requests.get(url, stream=True)
            for data in req.iter_content(4096):
                f.write(data)
        tar = tarfile.open(filename)
        bad_paths = filter(lambda name: name.startswith('/') or '..' in name,
                           tar.getnames())
        if any(bad_paths):
            raise Exception("Tar archive contains non-local paths,"
                            "e.g. '%s'" % bad_paths[0])
        self.log(logging.INFO, 'rm_vendor_dir', {}, 'rm -rf %s' % target)
        mozfile.remove(target)
        self.log(logging.INFO, 'unpack', {}, 'Unpacking upstream files.')
        tar.extractall(target)
        # Github puts everything properly down a directory; move it up.
        if all(map(lambda name: name.startswith(prefix), tar.getnames())):
            tardir = mozpath.join(target, prefix)
            os.system('mv %s/* %s/.* %s' % (tardir, tardir, target))
            os.rmdir(tardir)
        # Remove the tarball.
        mozfile.remove(filename)

    def update_yaml(self, revision, timestamp, target):
        filename = mozpath.join(target, 'moz.yaml')
        with open(filename) as f:
            yaml = f.read()

        prefix = '  release: commit'
        if prefix in yaml:
            new_yaml = re.sub(prefix + ' [v\.a-f0-9]+.*$',
                              prefix + ' %s (%s).' % (revision, timestamp),
                              yaml, flags=re.MULTILINE)
        else:
            new_yaml = '%s\n\n%s %s.' % (yaml, prefix, revision)

        if yaml != new_yaml:
            with open(filename, 'w') as f:
                f.write(new_yaml)

    def update_vcs_version(self, revision, vendor_dir, glue_dir):
        src_filename = mozpath.join(vendor_dir, 'include/vcs_version.h.in')
        dst_filename = mozpath.join(glue_dir, 'vcs_version.h')
        with open(src_filename) as f:
            vcs_version_in = f.read()
        vcs_version = vcs_version_in.replace('@VCS_TAG@', revision)
        with open(dst_filename, 'w') as f:
            f.write(vcs_version)

    def clean_upstream(self, target):
        '''Remove files we don't want to import.'''
        mozfile.remove(mozpath.join(target, '.gitattributes'))
        mozfile.remove(mozpath.join(target, '.gitignore'))
        mozfile.remove(mozpath.join(target, 'build', '.gitattributes'))
        mozfile.remove(mozpath.join(target, 'build', '.gitignore'))

    def check_modified_files(self):
        '''
        Ensure that there aren't any uncommitted changes to files
        in the working copy, since we're going to change some state
        on the user.
        '''
        modified = self.repository.get_changed_files('M')
        if modified:
            self.log(logging.ERROR, 'modified_files', {},
                     '''You have uncommitted changes to the following files:

{files}

Please commit or stash these changes before vendoring, or re-run with `--ignore-modified`.
'''.format(files='\n'.join(sorted(modified))))
            sys.exit(1)

    def vendor(self, revision, repo, ignore_modified=False):
        self.populate_logger()
        self.log_manager.enable_unstructured()

        if not ignore_modified:
            self.check_modified_files()
        if not revision:
            revision = 'master'
        if repo:
            self.repo_url = repo
        else:
            self.repo_url = 'https://code.videolan.org/videolan/dav1d'
        self.upstream_validate(self.repo_url)

        commit, timestamp = self.upstream_commit(revision)

        vendor_dir = mozpath.join(self.topsrcdir, 'third_party/dav1d')
        self.fetch_and_unpack(commit, vendor_dir)
        self.log(logging.INFO, 'clean_upstream', {},
                 '''Removing unnecessary files.''')
        self.clean_upstream(vendor_dir)
        glue_dir = mozpath.join(self.topsrcdir, 'media/libdav1d')
        self.log(logging.INFO, 'update_moz.yaml', {},
                 '''Updating moz.yaml.''')
        self.update_yaml(commit, timestamp, glue_dir)
        self.log(logging.INFO, 'update_vcs_version', {},
                 '''Updating vcs_version.h.''')
        self.update_vcs_version(commit, vendor_dir, glue_dir)
        self.log(logging.INFO, 'add_remove_files', {},
                 '''Registering changes with version control.''')
        self.repository.add_remove_files(vendor_dir, glue_dir)
        self.log(logging.INFO, 'done', {'revision': revision},
                 '''Update to dav1d version '{revision}' ready to commit.''')
