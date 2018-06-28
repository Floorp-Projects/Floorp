#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" l10n_bumper.py

    Updates a gecko repo with up to date changesets from l10n.mozilla.org.

    Specifically, it updates l10n-changesets.json which is used by mobile releases.

    This is to allow for `mach taskgraph` to reference specific l10n revisions
    without having to resort to task.extra or commandline base64 json hacks.
"""
import codecs
import os
import pprint
import sys
import time
try:
    import simplejson as json
    assert json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import HgErrorList
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.base.log import FATAL


class L10nBumper(VCSScript):
    config_options = [[
        ['--ignore-closed-tree', ],
        {
            "action": "store_true",
            "dest": "ignore_closed_tree",
            "default": False,
            "help": "Bump l10n changesets on a closed tree."
        }
    ], [
        ['--build', ],
        {
            "action": "store_false",
            "dest": "dontbuild",
            "default": True,
            "help": "Trigger new builds on push."
        }
    ]]

    def __init__(self, require_config_file=True):
        super(L10nBumper, self).__init__(
            all_actions=[
                'clobber',
                'check-treestatus',
                'checkout-gecko',
                'bump-changesets',
                'push',
                'push-loop',
            ],
            default_actions=[
                'push-loop',
            ],
            require_config_file=require_config_file,
            config_options=self.config_options,
            # Default config options
            config={
                'treestatus_base_url': 'https://treestatus.mozilla-releng.net',
                'log_max_rotate': 99,
            }
        )

    # Helper methods {{{1
    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(L10nBumper, self).query_abs_dirs()

        abs_dirs.update({
            'gecko_local_dir':
            os.path.join(
                abs_dirs['abs_work_dir'],
                self.config.get('gecko_local_dir', os.path.basename(self.config['gecko_pull_url']))
            ),
        })
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def hg_commit(self, path, repo_path, message):
        """
        Commits changes in repo_path, with specified user and commit message
        """
        user = self.config['hg_user']
        hg = self.query_exe('hg', return_type='list')
        env = self.query_env(partial_env={'LANG': 'en_US.UTF-8'})
        cmd = hg + ['add', path]
        self.run_command(cmd, cwd=repo_path, env=env)
        cmd = hg + ['commit', '-u', user, '-m', message]
        self.run_command(cmd, cwd=repo_path, env=env)

    def hg_push(self, repo_path):
        hg = self.query_exe('hg', return_type='list')
        command = hg + ["push", "-e",
                        "ssh -oIdentityFile=%s -l %s" % (
                            self.config["ssh_key"], self.config["ssh_user"],
                        ),
                        "-r", ".",
                        self.config["gecko_push_url"]]
        status = self.run_command(command, cwd=repo_path,
                                  error_list=HgErrorList)
        if status != 0:
            # We failed; get back to a known state so we can either retry
            # or fail out and continue later.
            self.run_command(hg + ["--config", "extensions.mq=",
                                   "strip", "--no-backup", "outgoing()"],
                             cwd=repo_path)
            self.run_command(hg + ["up", "-C"],
                             cwd=repo_path)
            self.run_command(hg + ["--config", "extensions.purge=",
                                   "purge", "--all"],
                             cwd=repo_path)
            return False
        return True

    def _read_json(self, path):
        contents = self.read_from_file(path)
        try:
            json_contents = json.loads(contents)
            return json_contents
        except ValueError:
            self.error("%s is invalid json!" % path)

    def _read_version(self, path):
        contents = self.read_from_file(path).split('\n')[0]
        return contents.split('.')

    def _build_locale_map(self, old_contents, new_contents):
        locale_map = {}
        for key in old_contents:
            if key not in new_contents:
                locale_map[key] = "removed"
        for k, v in new_contents.items():
            if old_contents.get(k, {}).get('revision') != v['revision']:
                locale_map[k] = v['revision']
            elif old_contents.get(k, {}).get('platforms') != v['platforms']:
                locale_map[k] = v['platforms']
        return locale_map

    def _build_platform_dict(self, bump_config):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        platform_dict = {}
        ignore_config = bump_config.get('ignore_config', {})
        for platform_config in bump_config['platform_configs']:
            path = os.path.join(repo_path, platform_config['path'])
            self.info("Reading %s for %s locales..." % (path, platform_config['platforms']))
            contents = self.read_from_file(path)
            for locale in contents.splitlines():
                # locale is 1st word in line in shipped-locales
                if platform_config.get('format') == 'shipped-locales':
                    locale = locale.split(' ')[0]
                existing_platforms = set(platform_dict.get(locale, {}).get('platforms', []))
                platforms = set(platform_config['platforms'])
                ignore_platforms = set(ignore_config.get(locale, []))
                platforms = (platforms | existing_platforms) - ignore_platforms
                platform_dict[locale] = {
                    'platforms': sorted(list(platforms))
                }
        self.info("Built platform_dict:\n%s" % pprint.pformat(platform_dict))
        return platform_dict

    def _build_revision_dict(self, bump_config, version_list):
        self.info("Building revision dict...")
        platform_dict = self._build_platform_dict(bump_config)
        revision_dict = {}
        if bump_config.get('revision_url'):
            repl_dict = {
                'MAJOR_VERSION': version_list[0],
            }

            url = bump_config['revision_url'] % repl_dict
            path = self.download_file(url, error_level=FATAL)
            revision_info = self.read_from_file(path)
            self.info("Got %s" % revision_info)
            for line in revision_info.splitlines():
                locale, revision = line.split(' ')
                if locale in platform_dict:
                    revision_dict[locale] = platform_dict[locale]
                    revision_dict[locale]['revision'] = revision
        else:
            for k, v in platform_dict.items():
                v['revision'] = 'default'
                revision_dict[k] = v
        self.info("revision_dict:\n%s" % pprint.pformat(revision_dict))
        return revision_dict

    def build_commit_message(self, name, locale_map):
        comments = ''
        approval_str = 'r=release a=l10n-bump'
        for locale, revision in sorted(locale_map.items()):
            comments += "%s -> %s\n" % (locale, revision)
        if self.config['dontbuild']:
            approval_str += " DONTBUILD"
        if self.config['ignore_closed_tree']:
            approval_str += " CLOSED TREE"
        message = 'no bug - Bumping %s %s\n\n' % (name, approval_str)
        message += comments
        message = message.encode("utf-8")
        return message

    def query_treestatus(self):
        "Return True if we can land based on treestatus"
        c = self.config
        dirs = self.query_abs_dirs()
        tree = c.get('treestatus_tree', os.path.basename(c['gecko_pull_url'].rstrip("/")))
        treestatus_url = "%s/trees/%s" % (c['treestatus_base_url'], tree)
        treestatus_json = os.path.join(dirs['abs_work_dir'], 'treestatus.json')
        if not os.path.exists(dirs['abs_work_dir']):
            self.mkdir_p(dirs['abs_work_dir'])
        self.rmtree(treestatus_json)

        self.run_command(["curl", "--retry", "4", "-o", treestatus_json, treestatus_url],
                         throw_exception=True)

        treestatus = self._read_json(treestatus_json)
        if treestatus['result']['status'] != 'closed':
            self.info("treestatus is %s - assuming we can land" %
                      repr(treestatus['result']['status']))
            return True

        return False

    # Actions {{{1
    def check_treestatus(self):
        if not self.config['ignore_closed_tree'] and not self.query_treestatus():
            self.info("breaking early since treestatus is closed")
            sys.exit(0)

    def checkout_gecko(self):
        c = self.config
        dirs = self.query_abs_dirs()
        dest = dirs['gecko_local_dir']
        repos = [{
            'repo': c['gecko_pull_url'],
            'tag': c.get('gecko_tag', 'default'),
            'dest': dest,
            'vcs': 'hg',
        }]
        self.vcs_checkout_repos(repos)

    def bump_changesets(self):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        version_path = os.path.join(repo_path, self.config['version_path'])
        changes = False
        version_list = self._read_version(version_path)
        for bump_config in self.config['bump_configs']:
            path = os.path.join(repo_path,
                                bump_config['path'])
            # For now, assume format == 'json'.  When we add desktop support,
            # we may need to add flatfile support
            if os.path.exists(path):
                old_contents = self._read_json(path)
            else:
                old_contents = {}

            new_contents = self._build_revision_dict(bump_config, version_list)

            if new_contents == old_contents:
                continue
            # super basic sanity check
            if not isinstance(new_contents, dict) or len(new_contents) < 5:
                self.error("Cowardly refusing to land a broken-seeming changesets file!")
                continue

            # Write to disk
            content_string = json.dumps(new_contents, sort_keys=True, indent=4)
            fh = codecs.open(path, encoding='utf-8', mode='w+')
            fh.write(content_string + "\n")
            fh.close()

            locale_map = self._build_locale_map(old_contents, new_contents)

            # Commit
            message = self.build_commit_message(bump_config['name'],
                                                locale_map)
            self.hg_commit(path, repo_path, message)
            changes = True
        return changes

    def push(self):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        return self.hg_push(repo_path)

    def push_loop(self):
        max_retries = 5
        for _ in range(max_retries):
            changed = False
            if not self.config['ignore_closed_tree'] and not self.query_treestatus():
                # Tree is closed; exit early to avoid a bunch of wasted time
                self.info("breaking early since treestatus is closed")
                break

            self.checkout_gecko()
            if self.bump_changesets():
                changed = True

            if not changed:
                # Nothing changed, we're all done
                self.info("No changes - all done")
                break

            if self.push():
                # We did it! Hurray!
                self.info("Great success!")
                break
            # If we're here, then the push failed. It also stripped any
            # outgoing commits, so we should be in a pristine state again
            # Empty our local cache of manifests so they get loaded again next
            # time through this loop. This makes sure we get fresh upstream
            # manifests, and avoids problems like bug 979080
            self.device_manifests = {}

            # Sleep before trying again
            self.info("Sleeping 60 before trying again")
            time.sleep(60)
        else:
            self.fatal("Didn't complete successfully (hit max_retries)")


# __main__ {{{1
if __name__ == '__main__':
    bumper = L10nBumper()
    bumper.run_and_exit()
