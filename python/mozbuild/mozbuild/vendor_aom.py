# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from distutils.version import LooseVersion
import logging
from mozbuild.base import (
    BuildEnvironmentNotFoundException,
    MozbuildObject,
)
import mozfile
import mozpack.path as mozpath
import os
import requests
import re
import sys
import tarfile
from urlparse import urlparse

class VendorAOM(MozbuildObject):
    def upstream_snapshot(self, revision):
        '''Construct a url for a tarball snapshot of the given revision.'''
        if 'googlesource' in self.repo_url:
            return mozpath.join(self.repo_url, '+archive', revision + '.tar.gz')
        elif 'github' in self.repo_url:
            return mozpath.join(self.repo_url, 'archive', revision + '.tar.gz')
        else:
            raise ValueError('Unknown git host, no snapshot lookup method')

    def upstream_commit(self, revision):
        '''Convert a revision to a git commit and timestamp.

        Ask the upstream repo to convert the requested revision to
        a git commit id and timestamp, so we can be precise in
        what we're vendoring.'''
        if 'googlesource' in self.repo_url:
            return self.upstream_googlesource_commit(revision)
        elif 'github' in self.repo_url:
            return self.upstream_github_commit(revision)
        else:
            raise ValueError('Unknown git host, no commit lookup method')

    def upstream_validate(self, url):
        '''Validate repository urls to make sure we can handle them.'''
        host = urlparse(url).netloc
        valid_domains = ('googlesource.com', 'github.com')
        if not any(filter(lambda domain: domain in host, valid_domains)):
            self.log(logging.ERROR, 'upstream_url', {},
                     '''Unsupported git host %s; cannot fetch snapshots.

Please set a repository url with --repo on either googlesource or github.''' % host)
            sys.exit(1)

    def upstream_googlesource_commit(self, revision):
        '''Query gitiles for a git commit and timestamp.'''
        url = mozpath.join(self.repo_url, '+', revision + '?format=JSON')
        self.log(logging.INFO, 'fetch', {'url': url},
                 'Fetching commit id from {url}')
        req = requests.get(url)
        req.raise_for_status()
        try:
            info = req.json()
        except ValueError as e:
            # As of 2017 May, googlesource sends 4 garbage characters
            # at the beginning of the json response. Work around this.
            # https://bugs.chromium.org/p/chromium/issues/detail?id=718550
            import json
            info = json.loads(req.text[4:])
        return (info['commit'], info['committer']['time'])

    def upstream_github_commit(self, revision):
        '''Query the github api for a git commit id and timestamp.'''
        github_api = 'https://api.github.com/'
        repo = urlparse(self.repo_url).path[1:]
        url = mozpath.join(github_api, 'repos', repo, 'commits', revision)
        self.log(logging.INFO, 'fetch', {'url': url},
                 'Fetching commit id from {url}')
        req = requests.get(url)
        req.raise_for_status()
        info = req.json()
        return (info['sha'], info['commit']['committer']['date'])

    def fetch_and_unpack(self, revision, target):
        '''Fetch and unpack upstream source'''
        url = self.upstream_snapshot(revision)
        self.log(logging.INFO, 'fetch', {'url': url}, 'Fetching {url}')
        prefix = 'aom-' + revision
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

    def update_readme(self, revision, timestamp, target):
        filename = mozpath.join(target, 'README_MOZILLA')
        with open(filename) as f:
            readme = f.read()

        prefix = 'The git commit ID used was'
        if prefix in readme:
            new_readme = re.sub(prefix + ' [v\.a-f0-9]+.*$',
                                prefix + ' %s (%s).' % (revision, timestamp),
                                readme)
        else:
            new_readme = '%s\n\n%s %s.' % (readme, prefix, revision)

        prefix = 'The last update was pulled from'
        new_readme = re.sub(prefix + ' https*://.*',
                            prefix + ' %s' % self.repo_url,
                            new_readme)

        if readme != new_readme:
            with open(filename, 'w') as f:
                f.write(new_readme)

    def clean_upstream(self, target):
        '''Remove files we don't want to import.'''
        mozfile.remove(mozpath.join(target, '.gitattributes'))
        mozfile.remove(mozpath.join(target, '.gitignore'))
        mozfile.remove(mozpath.join(target, 'build', '.gitattributes'))
        mozfile.remove(mozpath.join(target, 'build' ,'.gitignore'))

    def generate_sources(self, target):
        '''
        Run the library's native build system to update ours.

        Invoke configure for each supported platform to generate
        appropriate config and header files, then invoke the
        makefile to obtain a list of source files, writing
        these out in the appropriate format for our build
        system to use.
        '''
        config_dir = mozpath.join(target, 'config')
        self.log(logging.INFO, 'rm_confg_dir', {}, 'rm -rf %s' % config_dir)
        mozfile.remove(config_dir)
        self.run_process(args=['./generate_sources_mozbuild.sh'],
                         cwd=target, log_name='generate_sources')

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
            self.repo_url = 'https://aomedia.googlesource.com/aom/'
        self.upstream_validate(self.repo_url)

        commit, timestamp = self.upstream_commit(revision)

        vendor_dir = mozpath.join(self.topsrcdir, 'third_party/aom')
        self.fetch_and_unpack(commit, vendor_dir)
        self.log(logging.INFO, 'clean_upstream', {},
                 '''Removing unnecessary files.''')
        self.clean_upstream(vendor_dir)
        glue_dir = mozpath.join(self.topsrcdir, 'media/libaom')
        self.log(logging.INFO, 'generate_sources', {},
                 '''Generating build files...''')
        self.generate_sources(glue_dir)
        self.log(logging.INFO, 'update_readme', {},
                 '''Updating README_MOZILLA.''')
        self.update_readme(commit, timestamp, glue_dir)
        self.repository.add_remove_files(vendor_dir)
        self.log(logging.INFO, 'add_remove_files', {},
                 '''Registering changes with version control.''')
        self.repository.add_remove_files(vendor_dir)
        self.repository.add_remove_files(glue_dir)
        self.log(logging.INFO, 'done', {'revision': revision},
                 '''Update to aom version '{revision}' ready to commit.''')
