# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import re
import subprocess
import sys

import logging

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase

import mozpack.path as mozpath

import json


GITHUB_ROOT = 'https://github.com/'
PR_REPOSITORIES = {
    'webrender': {
        'github': 'servo/webrender',
        'path': 'gfx/wr',
        'bugzilla_product': 'Core',
        'bugzilla_component': 'Graphics: WebRender',
    },
    'debugger': {
        'github': 'firefox-devtools/debugger',
        'path': 'devtools/client/debugger',
        'bugzilla_product': 'DevTools',
        'bugzilla_component': 'Debugger'
    },
}


@CommandProvider
class PullRequestImporter(MachCommandBase):
    @Command('import-pr', category='misc',
             description='Import a pull request from Github to the local repo.')
    @CommandArgument('-b', '--bug-number',
                     help='Bug number to use in the commit messages.')
    @CommandArgument('-t', '--bugzilla-token',
                     help='Bugzilla API token used to file a new bug if no bug number is '
                          'provided.')
    @CommandArgument('-r', '--reviewer',
                     help='Reviewer nick to apply to commit messages.')
    @CommandArgument('pull_request',
                     help='URL to the pull request to import (e.g. '
                          'https://github.com/servo/webrender/pull/3665).')
    def import_pr(self, pull_request, bug_number=None, bugzilla_token=None, reviewer=None):
        import requests
        pr_number = None
        repository = None
        for r in PR_REPOSITORIES.values():
            if pull_request.startswith(GITHUB_ROOT + r['github'] + '/pull/'):
                # sanitize URL, dropping anything after the PR number
                pr_number = int(re.search('/pull/([0-9]+)', pull_request).group(1))
                pull_request = GITHUB_ROOT + r['github'] + '/pull/' + str(pr_number)
                repository = r
                break

        if repository is None:
            self.log(logging.ERROR, 'unrecognized_repo', {},
                     'The pull request URL was not recognized; add it to the list of '
                     'recognized repos in PR_REPOSITORIES in %s' % __file__)
            sys.exit(1)

        self.log(logging.INFO, 'import_pr', {'pr_url': pull_request},
                 'Attempting to import {pr_url}')
        dirty = [f for f in self.repository.get_changed_files(mode='all')
                 if f.startswith(repository['path'])]
        if dirty:
            self.log(logging.ERROR, 'dirty_tree', repository,
                     'Local {path} tree is dirty; aborting!')
            sys.exit(1)
        target_dir = mozpath.join(self.topsrcdir, os.path.normpath(repository['path']))

        if bug_number is None:
            if bugzilla_token is None:
                self.log(logging.WARNING, 'no_token', {},
                         'No bug number or bugzilla API token provided; bug number will not '
                         'be added to commit messages.')
            else:
                bug_number = self._file_bug(bugzilla_token, repository, pr_number)
        elif bugzilla_token is not None:
            self.log(logging.WARNING, 'too_much_bug', {},
                     'Providing a bugzilla token is unnecessary when a bug number is provided. '
                     'Using bug number; ignoring token.')

        pr_patch = requests.get(pull_request + '.patch')
        pr_patch.raise_for_status()
        for patch in self._split_patches(pr_patch.content, bug_number, pull_request, reviewer):
            self.log(logging.INFO, 'commit_msg', patch,
                     'Processing commit [{commit_summary}] by [{author}] at [{date}]')
            patch_cmd = subprocess.Popen(['patch', '-p1', '-s'], stdin=subprocess.PIPE,
                                         cwd=target_dir)
            patch_cmd.stdin.write(patch['diff'])
            patch_cmd.stdin.close()
            patch_cmd.wait()
            if patch_cmd.returncode is not 0:
                self.log(logging.ERROR, 'commit_fail', {},
                         'Error applying diff from commit via "patch -p1 -s". Aborting...')
                sys.exit(patch_cmd.returncode)
            self.repository.commit(patch['commit_msg'], patch['author'], patch['date'],
                                   [target_dir])
            self.log(logging.INFO, 'commit_pass', {},
                     'Committed successfully.')

    def _file_bug(self, token, repo, pr_number):
        import requests
        bug = requests.post('https://bugzilla.mozilla.org/rest/bug?api_key=%s' % token,
                            json={
                                'product': repo['bugzilla_product'],
                                'component': repo['bugzilla_component'],
                                'summary': 'Land %s#%s in mozilla-central' %
                                           (repo['github'], pr_number),
                                'version': 'unspecified',
                            })
        bug.raise_for_status()
        self.log(logging.DEBUG, 'new_bug', {}, bug.content)
        bugnumber = json.loads(bug.content)['id']
        self.log(logging.INFO, 'new_bug', {'bugnumber': bugnumber},
                 'Filed bug {bugnumber}')
        return bugnumber

    def _split_patches(self, patchfile, bug_number, pull_request, reviewer):
        INITIAL = 0
        COMMIT_MESSAGE_SUMMARY = 1
        COMMIT_MESSAGE_BODY = 2
        COMMIT_DIFF = 3

        state = INITIAL
        for line in patchfile.splitlines():
            if state == INITIAL:
                if line.startswith('From: '):
                    author = line[6:]
                elif line.startswith('Date: '):
                    date = line[6:]
                elif line.startswith('Subject: '):
                    line = line[9:]
                    bug_prefix = ''
                    if bug_number is not None:
                        bug_prefix = 'Bug %s - ' % bug_number
                    commit_msg = re.sub(r'^\[PATCH[0-9 /]*\] ', bug_prefix, line)
                    state = COMMIT_MESSAGE_SUMMARY
            elif state == COMMIT_MESSAGE_SUMMARY:
                if len(line) > 0 and line[0] == ' ':
                    # Subject line has wrapped
                    commit_msg += line
                else:
                    if reviewer is not None:
                        commit_msg += ' r=' + reviewer
                    commit_summary = commit_msg
                    commit_msg += '\n' + line + '\n'
                    state = COMMIT_MESSAGE_BODY
            elif state == COMMIT_MESSAGE_BODY:
                if line == '---':
                    commit_msg += '[import_pr] From ' + pull_request + '\n'
                    state = COMMIT_DIFF
                    diff = ''
                else:
                    commit_msg += line + '\n'
            elif state == COMMIT_DIFF:
                if line.startswith('From '):
                    patch = {
                        'author': author,
                        'date': date,
                        'commit_summary': commit_summary,
                        'commit_msg': commit_msg,
                        'diff': diff,
                    }
                    yield patch
                    state = INITIAL
                else:
                    diff += line + '\n'

        if state != COMMIT_DIFF:
            self.log(logging.ERROR, 'unexpected_eof', {},
                     'Unexpected EOF found while importing patchfile')
            sys.exit(1)

        patch = {
            'author': author,
            'date': date,
            'commit_summary': commit_summary,
            'commit_msg': commit_msg,
            'diff': diff,
        }
        yield patch
