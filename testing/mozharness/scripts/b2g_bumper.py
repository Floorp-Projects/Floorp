#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" b2g_bumper.py

    Updates a gecko repo with up to date information from B2G repositories.

    In particular, it updates gaia.json which is used by B2G desktop builds,
    and updates the XML manifests used by device builds.

    This is to tie the external repository revisions to a visible gecko commit
    which appears on TBPL, so sheriffs can blame the appropriate changes.
"""

import os
import sys
from multiprocessing.pool import ThreadPool
import subprocess
import time
from urlparse import urlparse
try:
    import simplejson as json
    assert json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import HgErrorList
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.mozilla import repo_manifest
from mozharness.base.log import ERROR
from mozharness.mozilla.mapper import MapperMixin


class B2GBumper(VCSScript, MapperMixin):
    config_options = [
        [['--no-write'], {
            'dest': 'do_write',
            'action': 'store_const',
            'const': False,
            'help': 'disable writing in-tree manifests',
        }],
        [['--device'], {
            'dest': 'device_override',
            'help': 'specific device to process',
        }],
    ]

    def __init__(self, require_config_file=True):
        super(B2GBumper, self).__init__(
            config_options=self.config_options,
            all_actions=[
                'delete-git-ref-cache',
                'import-git-ref-cache',
                'clobber',
                'check-treestatus',
                'checkout-gecko',
                'bump-gaia',
                'checkout-manifests',
                'massage-manifests',
                'commit-manifests',
                'push',
                'push-loop',
                'export-git-ref-cache',
            ],
            default_actions=[
                'push-loop',
            ],
            require_config_file=require_config_file,
            # Default config options
            config={
                'treestatus_base_url': 'https://treestatus.mozilla.org',
                'log_max_rotate': 99,
                'do_write': True,
            }
        )

        # Mapping of device name to manifest
        self.device_manifests = {}

        # Cache of "%s:%s" % (remote url, refname) to revision hashes
        self._git_ref_cache = {}

        # File location for persisting _git_ref_cache dictionary above as a json file
        self.git_ref_cache_file = self.config.get('git_ref_cache', os.path.join(self.query_abs_dirs()['abs_work_dir'], 'git_ref_cache.json'))

        # Cache of new remotes to original upstreams
        self._remote_mappings = {}

        # What's the latest gaia revsion we have for hg
        self.gaia_hg_revision = None
        self.gaia_git_rev = None

    # Helper methods {{{1
    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(B2GBumper, self).query_abs_dirs()

        abs_dirs.update({
            'manifests_dir':
            os.path.join(abs_dirs['abs_work_dir'], 'manifests'),
            'gecko_local_dir':
            os.path.join(abs_dirs['abs_work_dir'],
                         self.config['gecko_local_dir']),
        })
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_manifest(self, device_name):
        if device_name in self.device_manifests:
            return self.device_manifests[device_name]
        dirs = self.query_abs_dirs()
        c = self.config
        manifest_file = c['devices'][device_name].get('manifest_file',
                                                      '%s.xml' % device_name)
        manifest_file = os.path.join(dirs['manifests_dir'], manifest_file)
        self.info("Loading %s" % manifest_file)
        manifest = repo_manifest.load_manifest(manifest_file)
        self.device_manifests[device_name] = manifest
        return manifest

    def filter_projects(self, device_config, manifest):
        for p in device_config['ignore_projects']:
            removed = repo_manifest.remove_project(manifest, path=p)
            if removed:
                self.info("Removed %s" % removed.toxml())

    def filter_groups(self, device_config, manifest):
        for g in device_config.get('ignore_groups', []):
            removed = repo_manifest.remove_group(manifest, g)
            for r in removed:
                self.info("Removed %s" % r.toxml())

    def map_remotes(self, manifest):
        def mapping_func(r):
            orig_url = r.getAttribute('fetch')
            m = repo_manifest.map_remote(r, self.config['repo_remote_mappings'])
            self._remote_mappings[m.getAttribute('fetch')] = orig_url
            return m
        repo_manifest.rewrite_remotes(manifest, mapping_func)

    def resolve_git_ref(self, remote_url, revision):
        cache_key = "%s:%s" % (remote_url, revision)
        cmd = ['git', 'ls-remote', remote_url, revision]
        self.debug("Running %s" % cmd)
        # Retry this a few times, in case there are network errors or somesuch
        max_retries = 5
        for _ in range(max_retries):
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
            if proc.wait() != 0:
                self.warning("Returned %i - sleeping and retrying" %
                             proc.returncode)
                self.warning("%s - got output: %s" % (cache_key, proc.stdout.read()))
                time.sleep(30)
                continue
            output = proc.stdout.read()
            self.info("%s - got output: %s" % (cache_key, output))
            try:
                abs_revision = output.split()[0].strip()
                self._git_ref_cache[cache_key] = abs_revision
                return abs_revision
            except IndexError:
                # Couldn't split the output properly
                self.warning("no output from: git ls-remote %s %s" % (remote_url, revision))
                return None
        return None

    def resolve_refs(self, manifest):
        worker_pool = ThreadPool(20)
        lookup_threads_by_project = {}
        lookup_threads_by_parameters = {}

        # Resolve refnames
        for p in manifest.getElementsByTagName('project'):
            name = p.getAttribute('name')
            remote_url = repo_manifest.get_project_remote_url(manifest, p)
            revision = repo_manifest.get_project_revision(manifest, p)

            # commit ids are already done
            if repo_manifest.is_commitid(revision):
                self.debug("%s is already locked to %s; skipping" %
                           (name, revision))
                continue

            # gaia is special - make sure we're using the same revision we used
            # for gaia.json
            if self.gaia_hg_revision and p.getAttribute('path') == 'gaia' and revision == self.config['gaia_git_branch']:
                git_rev = self.query_gaia_git_rev()
                self.info("Using %s for gaia to match %s in gaia.json" % (git_rev, self.gaia_hg_revision))
                p.setAttribute('revision', git_rev)
                continue

            # If there's no '/' in the revision, assume it's a head
            if '/' not in revision:
                revision = 'refs/heads/%s' % revision

            cache_key = "%s:%s" % (remote_url, revision)

            # Check to see if we've looked up this revision on this remote
            # before. If we have, reuse the previous value rather than looking
            # it up again. This will make sure revisions for the same ref name
            # are consistent between devices, as long as they use the same
            # remote/refname.
            if cache_key in self._git_ref_cache:
                abs_revision = self._git_ref_cache[cache_key]
                self.debug(
                    "Reusing previous lookup %s -> %s" %
                    (cache_key, abs_revision))
                p.setAttribute('revision', abs_revision)
                continue

            # Maybe a thread already exists for this lookup, even if the result has not
            # yet been retrieved and placed in _git_ref_cache...
            # Please note result.get() can be called multiple times without problems;
            # the git command will only be executed once. Therefore we can associate many
            # projects to the same thread result, without problems later when we call
            # get() multiple times against the same thread result.
            if cache_key in lookup_threads_by_parameters:
                self.debug("Reusing currently running thread to look up %s" % cache_key)
                lookup_threads_by_project[p] = lookup_threads_by_parameters.get(cache_key)
            else:
                async_result = worker_pool.apply_async(self.resolve_git_ref,
                                                   (remote_url, revision))
                lookup_threads_by_parameters[cache_key] = async_result
                lookup_threads_by_project[p] = async_result

        # TODO: alert/notify on missing repositories
        abort = False
        failed = []
        for p, result in lookup_threads_by_project.iteritems():
            abs_revision = result.get(timeout=300)
            remote_url = repo_manifest.get_project_remote_url(manifest, p)
            revision = repo_manifest.get_project_revision(manifest, p)
            if not abs_revision:
                abort = True
                self.error("Couldn't resolve reference %s %s" % (remote_url, revision))
                failed.append(p)
            p.setAttribute('revision', abs_revision)
        if abort:
            # Write message about how to set up syncing
            default = repo_manifest.get_default(manifest)
            for p in failed:
                if p.hasAttribute('remote'):
                    remote = repo_manifest.get_remote(manifest, p.getAttribute('remote'))
                else:
                    remote = repo_manifest.get_remote(manifest, default.getAttribute('remote'))

                new_fetch_url = remote.getAttribute('fetch')
                orig_fetch_url = self._remote_mappings[new_fetch_url]
                name = p.getAttribute('name')
                self.info("needs sync? %s/%s -> %s/%s" % (orig_fetch_url, name, new_fetch_url, name))

            self.fatal("couldn't resolve some refs; exiting")

    def query_manifest_path(self, device):
        dirs = self.query_abs_dirs()
        device_config = self.config['devices'][device]
        manifest_file = os.path.join(
            dirs['gecko_local_dir'],
            'b2g', 'config',
            device_config.get('gecko_device_dir', device),
            'sources.xml')
        return manifest_file

    def hg_add(self, repo_path, path):
        """
        Runs 'hg add' on path
        """
        hg = self.query_exe('hg', return_type='list')
        cmd = hg + ['add', path]
        self.run_command(cmd, cwd=repo_path)

    def hg_commit(self, repo_path, message):
        """
        Commits changes in repo_path, with specified user and commit message
        """
        user = self.config['hg_user']
        hg = self.query_exe('hg', return_type='list')
        cmd = hg + ['commit', '-u', user, '-m', message]
        env = self.query_env(partial_env={'LANG': 'en_US.UTF-8'})
        status = self.run_command(cmd, cwd=repo_path, env=env)
        return status == 0

    def hg_push(self, repo_path):
        hg = self.query_exe('hg', return_type='list')
        command = hg + ["push", "-e",
                        "ssh -oIdentityFile=%s -l %s" % (
                            self.config["ssh_key"], self.config["ssh_user"],
                        ),
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
        if not os.path.exists(path):
            self.error("%s doesn't exist!" % path)
            return
        contents = self.read_from_file(path)
        try:
            json_contents = json.loads(contents)
            return json_contents
        except ValueError:
            self.error("%s is invalid json!" % path)

    def get_revision_list(self, repo_config, prev_revision=None):
        revision_list = []
        url = repo_config['polling_url']
        branch = repo_config.get('branch', 'default')
        max_revisions = self.config['gaia_max_revisions']
        dirs = self.query_abs_dirs()
        if prev_revision:
            # hgweb json-pushes hardcode
            url += '&fromchange=%s' % prev_revision
        file_name = os.path.join(dirs['abs_work_dir'],
                                 '%s.json' % repo_config['repo_name'])
        # might be nice to have a load-from-url option; til then,
        # download then read
        if self.retry(
            self.download_file,
            args=(url, ),
            kwargs={'file_name': file_name},
            error_level=ERROR,
            sleeptime=0,
        ) != file_name:
            return None
        contents = self.read_from_file(file_name)
        revision_dict = json.loads(contents)
        if not revision_dict:
            return []
        # Discard any revisions not on the branch we care about.
        for k in sorted(revision_dict, key=int):  # numeric sort
            v = revision_dict[k]
            if v['changesets'][-1]['branch'] == branch:
                revision_list.append(v)
        # Limit the list to max_revisions.
        return revision_list[:max_revisions]

    def update_gaia_json(self, path,
                         hg_revision, hg_repo_path,
                         git_revision, git_repo,
                         ):
        """ Update path with repo_path + revision.

            If the revision hasn't changed, don't do anything.
            If the repo_path changes or the current json is invalid, error but don't fail.
            """
        if not os.path.exists(path):
            self.add_summary(
                "%s doesn't exist; can't update with repo_path %s revision %s!" %
                (path, hg_repo_path, hg_revision),
                level=ERROR,
            )
            return -1
        contents = self._read_json(path)
        if contents:
            if contents.get("repo_path") != hg_repo_path:
                self.error("Current repo_path %s differs from %s!" % (str(contents.get("repo_path")), hg_repo_path))
            if contents.get("revision") == hg_revision:
                self.info("Revision %s is the same.  No action needed." % hg_revision)
                self.add_summary("Revision %s is unchanged for repo_path %s." % (hg_revision, hg_repo_path))
                return 0
        contents = {
            "repo_path": hg_repo_path,
            "revision": hg_revision,
            "git": {
                "remote": git_repo,
                "branch": "",
                "git_revision": git_revision,
            }
        }
        if self.write_to_file(path, json.dumps(contents, indent=4) + "\n") != path:
            self.add_summary(
                "Unable to update %s with new revision %s!" % (path, hg_revision),
                level=ERROR,
            )
            return -2

    def build_commit_message(self, revision_list, repo_name, repo_url):
        revisions = []
        comments = ''
        for revision_config in reversed(revision_list):
            for changeset_config in reversed(revision_config['changesets']):
                revisions.append(changeset_config['node'])
                comments += "\n========\n"
                comments += u'\n%s/rev/%s\nAuthor: %s\nDesc: %s\n' % (
                    repo_url,
                    changeset_config['node'][:12],
                    changeset_config['author'],
                    changeset_config['desc'],
                )
        message = 'Bumping gaia.json for %d %s revision(s) a=gaia-bump\n' % (
            len(revisions),
            repo_name
        )
        message += comments
        message = message.encode("utf-8")
        return message

    def query_treestatus(self):
        "Return True if we can land based on treestatus"
        c = self.config
        dirs = self.query_abs_dirs()
        tree = c.get('treestatus_tree', os.path.basename(c['gecko_pull_url'].rstrip("/")))
        treestatus_url = "%s/%s?format=json" % (c['treestatus_base_url'], tree)
        treestatus_json = os.path.join(dirs['abs_work_dir'], 'treestatus.json')
        if not os.path.exists(dirs['abs_work_dir']):
            self.mkdir_p(dirs['abs_work_dir'])

        if self.download_file(treestatus_url, file_name=treestatus_json) != treestatus_json:
            # Failed to check tree status...assume we can land
            self.info("failed to check tree status - assuming we can land")
            return True

        treestatus = self._read_json(treestatus_json)
        if treestatus['status'] != 'closed':
            self.info("treestatus is %s - assuming we can land" % repr(treestatus['status']))
            return True

        return False

    def query_devices(self):
        c = self.config
        override = c.get('device_override')
        if override:
            return {override: c['devices'][override]}
        else:
            return c['devices']


    def query_gaia_git_rev(self):
        """Returns (and caches) the git revision for gaia corresponding to the
        latest hg revision on our branch."""
        if not self.gaia_hg_revision:
            return None

        if not self.gaia_git_rev:
            self.gaia_git_rev = self.query_mapper_git_revision(
                self.config['mapper_url'],
                self.config['gaia_mapper_project'],
                self.gaia_hg_revision,
            )
        return self.gaia_git_rev

    # Actions {{{1
    def check_treestatus(self):
        if not self.query_treestatus():
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
            'vcs': 'hgtool',
            'hgtool_base_bundle_urls': c.get('hgtool_base_bundle_urls'),
        }]
        self.vcs_checkout_repos(repos)

    def checkout_manifests(self):
        c = self.config
        dirs = self.query_abs_dirs()
        repos = [
            {'vcs': 'gittool',
             'repo': c['manifests_repo'],
             'revision': c['manifests_revision'],
             'dest': dirs['manifests_dir']},
        ]
        self.vcs_checkout_repos(repos)

    def massage_manifests(self):
        """
        For each device in config['devices'], we'll strip projects mentioned in
        'ignore_projects', or that have group attribute mentioned in
        'filter_groups'.
        We'll also map remote urls
        Finally, we'll resolve absolute refs for projects that aren't fully
        specified.
        """
        for device, device_config in self.query_devices().items():
            self.info("Massaging manifests for %s" % device)
            manifest = self.query_manifest(device)
            self.filter_projects(device_config, manifest)
            self.filter_groups(device_config, manifest)
            self.map_remotes(manifest)
            self.resolve_refs(manifest)
            repo_manifest.cleanup(manifest)
            self.device_manifests[device] = manifest

            manifest_path = self.query_manifest_path(device)
            manifest_xml = manifest.toxml()
            if not manifest_xml.endswith("\n"):
                manifest_xml += "\n"

            if self.config['do_write']:
                self.mkdir_p(os.path.dirname(manifest_path))
                self.write_to_file(manifest_path, manifest_xml)

    def commit_manifests(self):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        for device, device_config in self.query_devices().items():
            manifest_path = self.query_manifest_path(device)
            self.hg_add(repo_path, manifest_path)

        message = "Bumping manifests a=b2g-bump"
        return self.hg_commit(repo_path, message)

    def bump_gaia(self):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        gaia_json_path = os.path.join(repo_path,
                                      self.config['gaia_revision_file'])
        contents = self._read_json(gaia_json_path)

        # Get the list of changes
        if contents:
            prev_revision = contents.get('revision')
            self.gaia_hg_revision = prev_revision
        else:
            prev_revision = None

        polling_url = "%s/json-pushes?full=1" % self.config['gaia_repo_url']
        repo_config = {
            'polling_url': polling_url,
            'branch': self.config.get('gaia_branch', 'default'),
            'repo_name': 'gaia',
        }
        revision_list = self.get_revision_list(repo_config=repo_config,
                                               prev_revision=prev_revision)
        if not revision_list:
            # No changes
            return False

        # Update the gaia.json with the list of changes
        hg_gaia_repo_path = urlparse(self.config['gaia_repo_url']).path.lstrip('/')
        hg_revision = revision_list[-1]['changesets'][-1]['node']
        self.gaia_hg_revision = hg_revision

        git_revision = self.query_gaia_git_rev()
        git_gaia_repo = self.config['gaia_git_repo']

        self.update_gaia_json(gaia_json_path,
                              hg_revision, hg_gaia_repo_path,
                              git_revision, git_gaia_repo,
                              )

        # Commit
        message = self.build_commit_message(revision_list, 'gaia',
                                            self.config['gaia_repo_url'])
        self.hg_commit(repo_path, message)
        return True

    def push(self):
        dirs = self.query_abs_dirs()
        repo_path = dirs['gecko_local_dir']
        return self.hg_push(repo_path)

    def push_loop(self):
        max_retries = 5
        for _ in range(max_retries):
            changed = False
            if not self.query_treestatus():
                # Tree is closed; exit early to avoid a bunch of wasted time
                self.info("breaking early since treestatus is closed")
                break

            self.checkout_gecko()
            if not self.config.get('skip_gaia_json') and self.bump_gaia():
                changed = True
            self.checkout_manifests()
            self.massage_manifests()
            if self.commit_manifests():
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

    def import_git_ref_cache(self):
        """ This action imports the git ref cache created during a previous run. This is
        useful for sharing the cache across multiple branches (for example).
        """
        if os.path.exists(self.git_ref_cache_file):
            self._git_ref_cache = self._read_json(self.git_ref_cache_file)

    def export_git_ref_cache(self):
        """ This action exports the git ref cache created during this run. This is useful
        for sharing the cache across multiple branches (for example).
        """
        if self.write_to_file(self.git_ref_cache_file, json.dumps(self._git_ref_cache, sort_keys=True, indent=4) + "\n") != self.git_ref_cache_file:
            self.add_summary(
                "Unable to update %s with git ref cache" % self.git_ref_cache_file,
                level=ERROR,
            )
            return -2

    def delete_git_ref_cache(self):
        """ Used to delete the git ref cache from the file system. The cache can be used
        to persist git ls-remote lookup results, for example to reuse them between b2g bumper
        runs. Since the results are stale and do not get updated, the cache should be
        periodically deleted, so that the new refs can be fetched. The cache can also be used
        across branches/devices.
        """
        self.log("Deleting git ls-remote look-up cache file ('%s')...")
        os.remove(self.git_ref_cache_file)

# __main__ {{{1
if __name__ == '__main__':
    bumper = B2GBumper()
    bumper.run_and_exit()
