#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""vcs_sync.py

hg<->git conversions.  Needs to support both the monolithic beagle/gecko.git
type conversions, as well as many-to-many (l10n, build repos, etc.)
"""

from copy import deepcopy
import mmap
import os
import pprint
import re
import sys
import time

try:
    import simplejson as json
    assert json
except ImportError:
    import json

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

import mozharness
external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

from mozharness.base.errors import HgErrorList, GitErrorList
from mozharness.base.log import INFO, ERROR, FATAL
from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcssync import VCSSyncScript
from mozharness.mozilla.tooltool import TooltoolMixin


# HgGitScript {{{1
class HgGitScript(VirtualenvMixin, TooltoolMixin, TransferMixin, VCSSyncScript):
    """ Beagle-oriented hg->git script (lots of mozilla-central hardcodes;
        assumption that we're going to be importing lots of branches).

        Beagle is a git repo of mozilla-central, with full cvs history,
        and a number of developer-oriented repositories and branches added.

        The partner-oriented gecko.git could also be incorporated into this
        script with some changes.
        """

    mapfile_binary_search = None
    all_repos = None
    successful_repos = []
    config_options = [
        [["--no-check-incoming", ], {
            "action": "store_false",
            "dest": "check_incoming",
            "default": True,
            "help": "Don't check for incoming changesets",
        }],
        [["--max-log-sample-size", ], {
            "action": "store",
            "dest": "email_max_log_sample_size",
            "type": "int",
            "default": 102400,
            "help": "Specify the maximum number of characters from a log file to be "
                    "embedded in the email body, per embedding (not total - note we "
                    "embed two separate log samples into the email - so maximum "
                    "email body size can end up a little over 2x this amount).",
         }],
    ]

    def __init__(self, require_config_file=True):
        super(HgGitScript, self).__init__(
            config_options=virtualenv_config_options + self.config_options,
            all_actions=[
                'clobber',
                'list-repos',
                'create-virtualenv',
                'update-stage-mirror',
                'update-work-mirror',
                'create-git-notes',
                'publish-to-mapper',
                'push',
                'combine-mapfiles',
                'upload',
                'notify',
            ],
            # These default actions are the update loop that we run after the
            # initial steps to create the work mirror with all the branches +
            # cvs history have been run.
            default_actions=[
                'list-repos',
                'create-virtualenv',
                'update-stage-mirror',
                'update-work-mirror',
                'push',
                'combine-mapfiles',
                'upload',
                'notify',
            ],
            require_config_file=require_config_file
        )
        self.remote_targets = None

    # Helper methods {{{1
    def query_abs_dirs(self):
        """ Define paths.
            """
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(HgGitScript, self).query_abs_dirs()
        abs_dirs['abs_cvs_history_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'mozilla-cvs-history')
        abs_dirs['abs_source_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'stage_source')
        abs_dirs['abs_repo_sync_tools_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'repo-sync-tools')
        abs_dirs['abs_git_rewrite_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'mc-git-rewrite')
        abs_dirs['abs_target_dir'] = os.path.join(abs_dirs['abs_work_dir'],
                                                  'target')
        if 'conversion_dir' in self.config:
            abs_dirs['abs_conversion_dir'] = os.path.join(
                abs_dirs['abs_work_dir'], 'conversion',
                self.config['conversion_dir']
            )
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def init_git_repo(self, path, additional_args=None, deny_deletes=False):
        """ Create a git repo, with retries.

            We call this with additional_args=['--bare'] to save disk +
            make things cleaner.
            """
        git = self.query_exe("git", return_type="list")
        cmd = git + ['init']
        # generally for --bare
        if additional_args:
            cmd.extend(additional_args)
        cmd.append(path)
        status = self.retry(
            self.run_command,
            args=(cmd, ),
            error_level=FATAL,
            error_message="Can't set up %s!" % path
        )
        status = self.run_command(
            git + ['config', 'receive.denyNonFastForwards', 'true'],
            cwd=path
        )
        if deny_deletes:
            status = self.run_command(
                git + ['config', 'receive.denyDeletes', 'true'],
                cwd=path
            )
        return status

    def write_hggit_hgrc(self, dest):
        """ Update .hg/hgrc, if not already updated
            """
        hgrc = os.path.join(dest, '.hg', 'hgrc')
        contents = ''
        if os.path.exists(hgrc):
            contents = self.read_from_file(hgrc)
        if 'hggit=' not in contents:
            hgrc_update = """[extensions]
hggit=
[git]
intree=1
"""
            self.write_to_file(hgrc, hgrc_update, open_mode='a')

    def _process_locale(self, locale, type, config, l10n_remote_targets, name, l10n_repos):
        """ This contains the common processing that we do on both gecko_config
            and gaia_config for a given locale.
            """
        replace_dict = {'locale': locale}
        new_targets = deepcopy(config.get('targets', {}))
        for target in new_targets:
            dest = target['target_dest']
            if '%(locale)s' in dest:
                new_dest = dest % replace_dict
                target['target_dest'] = new_dest
                remote_target = l10n_remote_targets.get(new_dest)
                if remote_target is None:  # generate target if not seen before
                    possible_remote_target = l10n_remote_targets.get(dest)
                    # target may be remote, or local - we can tell by seeing
                    # local targets will be none - remote targets will not,
                    # so we can use this to test if it is local or remote
                    if possible_remote_target is not None:
                        remote_target = deepcopy(possible_remote_target)
                        remote_repo = remote_target.get('repo')
                        if '%(locale)s' in remote_repo:
                            remote_target['repo'] = remote_repo % replace_dict
                        l10n_remote_targets[new_dest] = remote_target
        long_name = '%s_%s_%s' % (type, name, locale)
        repo_dict = {
            'repo': config['hg_url'] % replace_dict,
            'revision': 'default',
            'repo_name': long_name,
            'conversion_dir': long_name,
            'mapfile_name': '%s-mapfile' % long_name,
            'targets': new_targets,
            'vcs': 'hg',
            'branch_config': {
                'branches': {
                    'default': config['git_branch_name'],
                },
            },
            'tag_config': config.get('tag_config', {}),
            'mapper': config.get('mapper', {}),
            'generate_git_notes': config.get('generate_git_notes', {}),
        }
        l10n_repos.append(repo_dict)

    def _query_l10n_repos(self):
        """ Since I didn't want to have to build a huge static list of l10n
            repos, and since it would be nicest to read the list of locales
            from their SSoT files.
            """
        l10n_repos = []
        l10n_remote_targets = deepcopy(self.config['remote_targets'])
        dirs = self.query_abs_dirs()
        gecko_dict = deepcopy(self.config['l10n_config'].get('gecko_config', {}))
        for name, gecko_config in gecko_dict.items():
            file_name = self.download_file(gecko_config['locales_file_url'],
                                           parent_dir=dirs['abs_work_dir'])
            if not os.path.exists(file_name):
                self.error("Can't download locales from %s; skipping!" % gecko_config['locales_file_url'])
                continue
            contents = self.read_from_file(file_name)
            for locale in contents.splitlines():
                self._process_locale(locale, 'gecko', gecko_config, l10n_remote_targets, name, l10n_repos)

        gaia_dict = deepcopy(self.config['l10n_config'].get('gaia_config', {}))
        for name, gaia_config in gaia_dict.items():
            contents = self.retry(
                self.load_json_from_url,
                args=(gaia_config['locales_file_url'],)
            )
            if not contents:
                self.error("Can't download locales from %s; skipping!" % gaia_config['locales_file_url'])
                continue
            for locale in dict(contents).keys():
                self._process_locale(locale, 'gaia', gaia_config, l10n_remote_targets, name, l10n_repos)
        self.info("Built l10n_repos...")
        self.info(pprint.pformat(l10n_repos, indent=4))
        self.info("Remote targets...")
        self.info(pprint.pformat(l10n_remote_targets, indent=4))
        self.remote_targets = l10n_remote_targets
        return l10n_repos

    def _query_project_repos(self):
        """ Since I didn't want to have to build a huge static list of project
            branch repos.
            """
        project_repos = []
        for project in self.config.get("project_branches", []):
            repo_dict = {
                'repo': self.config['project_branch_repo_url'] % {'project': project},
                'revision': 'default',
                'repo_name': project,
                'targets': [{
                    'target_dest': 'github-project-branches',
                }],
                'vcs': 'hg',
                'branch_config': {
                    'branches': {
                        'default': project,
                    },
                },
                'tag_config': {},
            }
            project_repos.append(repo_dict)
        self.info("Built project_repos...")
        self.info(pprint.pformat(project_repos, indent=4))
        return project_repos

    def query_all_repos(self):
        """ Very simple method, but we need this concatenated list many times
            throughout the script.
            """
        if self.all_repos:
            return self.all_repos
        if self.config.get('conversion_type') == 'b2g-l10n':
            self.all_repos = self._query_l10n_repos()
        elif self.config.get('initial_repo'):
            self.all_repos = [self.config['initial_repo']] + list(self.config.get('conversion_repos', []))
        else:
            self.all_repos = list(self.config.get('conversion_repos', []))
        if self.config.get('conversion_type') == 'project-branches':
            self.all_repos += self._query_project_repos()
        return self.all_repos

    def query_all_non_failed_repos(self):
        """ Same as query_all_repos(self) but filters out repos that failed in an earlier
            action - so use this for downstream actions that require earlier actions did
            not fail for a given repo.
            """
        all_repos = self.query_all_repos()
        return [repo for repo in all_repos if repo.get('repo_name') not in self.failures]

    def _query_repo_previous_status(self, repo_name, repo_map=None):
        """ Return False if previous run was unsuccessful.
            Return None if no previous run information.
            """
        if repo_map is None:
            repo_map = self._read_repo_update_json()
        return repo_map.get('repos', {}).get(repo_name, {}).get('previous_push_successful')

    def _update_repo_previous_status(self, repo_name, successful_flag, repo_map=None, write_update=False):
        """ Set the repo_name to successful_flag (False for unsuccessful, True for successful)
            """
        if repo_map is None:
            repo_map = self._read_repo_update_json()
        repo_map.setdefault('repos', {}).setdefault(repo_name, {})['previous_push_successful'] = successful_flag
        if write_update:
            self._write_repo_update_json(repo_map)
        return repo_map

    def _update_stage_repo(self, repo_config, retry=True, clobber=False):
        """ Update a stage repo.
            See update_stage_mirror() for a description of the stage repos.
            """
        hg = self._query_hg_exe()
        dirs = self.query_abs_dirs()
        repo_name = repo_config['repo_name']
        source_dest = os.path.join(dirs['abs_source_dir'],
                                   repo_name)
        if clobber:
            self.rmtree(source_dest)
        if not os.path.exists(source_dest):
            if self.retry(
                self.run_command,
                args=(hg + ['clone', '--noupdate', repo_config['repo'],
                      source_dest], ),
                kwargs={
                    'output_timeout': 15 * 60,
                    'cwd': dirs['abs_work_dir'],
                    'error_list': HgErrorList,
                },
            ):
                if retry:
                    return self._update_stage_repo(
                        repo_config, retry=False, clobber=True)
                else:
                    # Don't leave a failed clone behind
                    self.rmtree(source_dest)
                    self._update_repo_previous_status(repo_name, successful_flag=False, write_update=True)
                    self.add_failure(
                        repo_name,
                        message="Can't clone %s!" % repo_config['repo'],
                        level=ERROR,
                    )
        elif self.config['check_incoming'] and repo_config.get("check_incoming", True):
            previous_status = self._query_repo_previous_status(repo_name)
            if previous_status is None:
                self.info("No previous status for %s; skipping incoming check!" % repo_name)
            elif previous_status is False:
                self.info("Previously unsuccessful status for %s; skipping incoming check!" % repo_name)
            else:
                # Run |hg incoming| and skip all subsequent actions if there
                # are no no changes.
                # If you want to bypass this behavior (e.g. to update branches/tags
                # on a repo without requiring a new commit), set
                # repo_config["incoming_check"] = False.
                cmd = hg + ['incoming', '-n', '-l', '1']
                status = self.retry(
                    self.run_command,
                    args=(cmd, ),
                    kwargs={
                        'output_timeout': 5 * 60,
                        'cwd': source_dest,
                        'error_list': HgErrorList,
                        'success_codes': [0, 1, 256],
                    },
                )
                if status in (1, 256):
                    self.info("No changes for %s; skipping." % repo_name)
                    # Overload self.failures to tell downstream actions to noop on
                    # this repo
                    self.failures.append(repo_name)
                    return
                elif status != 0:
                    self.add_failure(
                        repo_name,
                        message="Error getting changes for %s; skipping!" % repo_config['repo_name'],
                        level=ERROR,
                    )
                    self._update_repo_previous_status(repo_name, successful_flag=False, write_update=True)
                    return
        cmd = hg + ['pull']
        if self.retry(
            self.run_command,
            args=(cmd, ),
            kwargs={
                'output_timeout': 15 * 60,
                'cwd': source_dest,
                'error_list': HgErrorList,
            },
        ):
            if retry:
                return self._update_stage_repo(
                    repo_config, retry=False, clobber=True)
            else:
                self._update_repo_previous_status(repo_name, successful_flag=False, write_update=True)
                self.add_failure(
                    repo_name,
                    message="Can't pull %s!" % repo_config['repo'],
                    level=ERROR,
                )
        # commenting out hg verify since it takes ~5min per repo; hopefully
        # exit codes will save us
#        if self.run_command(hg + ["verify"], cwd=source_dest):
#            if retry:
#                return self._update_stage_repo(repo_config, retry=False, clobber=True)
#            else:
#                self.fatal("Can't verify %s!" % source_dest)

    def _do_push_repo(self, base_command, refs_list=None, kwargs=None):
        """ Helper method for _push_repo() since it has to be able to break
            out of the target_repo list loop, and the commands loop borks that.
            """
        commands = []
        if refs_list:
            while len(refs_list) > 10:
                commands.append(base_command + refs_list[0:10])
                refs_list = refs_list[10:]
            commands.append(base_command + refs_list)
        else:
            commands = [base_command]
        if kwargs is None:
            kwargs = {}
        for command in commands:
            # Do the push, with retry!
            if self.retry(
                self.run_command,
                args=(command, ),
                kwargs=kwargs,
            ):
                return -1

    def _push_repo(self, repo_config):
        """ Push a repo to a path ("test_push") or remote server.

            This was meant to be a cross-vcs method, but currently only
            covers git pushes.
            """
        dirs = self.query_abs_dirs()
        conversion_dir = self.query_abs_conversion_dir(repo_config)
        if not conversion_dir:
            self.fatal("No conversion_dir for %s!" % repo_config['repo_name'])
        source_dir = os.path.join(dirs['abs_source_dir'], repo_config['repo_name'])
        git = self.query_exe('git', return_type='list')
        hg = self._query_hg_exe()
        return_status = ''
        for target_config in repo_config['targets']:
            test_push = False
            remote_config = {}
            if target_config.get("test_push"):
                test_push = True
                force_push = target_config.get("force_push")
                target_name = os.path.join(
                    dirs['abs_target_dir'], target_config['target_dest'])
                target_vcs = target_config.get("vcs")
            else:
                target_name = target_config['target_dest']
                if self.remote_targets is None:
                    self.remote_targets = self.config.get('remote_targets', {})
                remote_config = self.remote_targets.get(target_name, target_config)
                force_push = remote_config.get("force_push", target_config.get("force_push"))
                target_vcs = remote_config.get("vcs", target_config.get("vcs"))
            if target_vcs == "git":
                base_command = git + ['push']
                env = {}
                if force_push:
                    base_command.append("-f")
                if test_push:
                    target_git_repo = target_name
                else:
                    target_git_repo = remote_config['repo']
                    # Allow for using a custom git ssh key.
                    env['GIT_SSH_KEY'] = remote_config['ssh_key']
                    env['GIT_SSH'] = os.path.join(external_tools_path, 'git-ssh-wrapper.sh')
                base_command.append(target_git_repo)
                # Allow for pushing a subset of repo branches to the target.
                # If we specify that subset, we can also specify different
                # names for those branches (e.g. b2g18 -> master for a
                # standalone b2g18 repo)
                # We query hg for these because the conversion dir will have
                # branches from multiple hg repos, and the regexes may match
                # too many things.
                refs_list = []
                if repo_config.get('generate_git_notes', False):
                    refs_list.append('+refs/notes/commits:refs/notes/commits')
                branch_map = self.query_branches(
                    target_config.get('branch_config', repo_config.get('branch_config', {})),
                    source_dir,
                )
                # If the target_config has a branch_config, the key is the
                # local git branch and the value is the target git branch.
                if target_config.get("branch_config"):
                    for (branch, target_branch) in branch_map.items():
                        refs_list += ['+refs/heads/%s:refs/heads/%s' % (branch, target_branch)]
                # Otherwise the key is the hg branch and the value is the git
                # branch; use the git branch for both local and target git
                # branch names.
                else:
                    for (hg_branch, git_branch) in branch_map.items():
                        refs_list += ['+refs/heads/%s:refs/heads/%s' % (git_branch, git_branch)]
                # Allow for pushing a subset of tags to the target, via name or
                # regex.  Again, query hg for this list because the conversion
                # dir will contain tags from multiple hg repos, and the regexes
                # may match too many things.
                tag_config = target_config.get('tag_config', repo_config.get('tag_config', {}))
                if tag_config.get('tags'):
                    for (tag, target_tag) in tag_config['tags'].items():
                        refs_list += ['+refs/tags/%s:refs/tags/%s' % (tag, target_tag)]
                if tag_config.get('tag_regexes'):
                    regex_list = []
                    for regex in tag_config['tag_regexes']:
                        regex_list.append(re.compile(regex))
                    tag_list = self.get_output_from_command(
                        hg + ['tags'],
                        cwd=source_dir,
                    )
                    if tag_list is not None:
                        for tag_line in tag_list.splitlines():
                            if not tag_line:
                                continue
                            tag_parts = tag_line.split()
                            if not tag_parts:
                                self.error("Bogus tag_line? %s" % str(tag_line))
                                continue
                            tag_name = tag_parts[0]
                            for regex in regex_list:
                                if tag_name != 'tip' and regex.search(tag_name) is not None:
                                    refs_list += ['+refs/tags/%s:refs/tags/%s' % (tag_name, tag_name)]
                                    continue
                error_msg = "%s: Can't push %s to %s!\n" % (repo_config['repo_name'], conversion_dir, target_git_repo)
                if self._do_push_repo(
                    base_command,
                    refs_list=refs_list,
                    kwargs={
                        'output_timeout': target_config.get("output_timeout", 30 * 60),
                        'cwd': os.path.join(conversion_dir, '.git'),
                        'error_list': GitErrorList,
                        'partial_env': env,
                    }
                ):
                    if target_config.get("test_push"):
                        error_msg += "This was a test push that failed; not proceeding any further with %s!\n" % repo_config['repo_name']
                    self.error(error_msg)
                    return_status += error_msg
                    if target_config.get("test_push"):
                        break
            else:
                # TODO write hg
                error_msg = "%s: Don't know how to deal with vcs %s!\n" % (
                    target_config['target_dest'], target_vcs)
                self.error(error_msg)
                return_status += error_msg
        return return_status

    def _query_mapped_revision(self, revision=None, mapfile=None):
        """ Use the virtualenv mapper module to search a mapfile for a
            revision.
            """
        if not callable(self.mapfile_binary_search):
            site_packages_path = self.query_python_site_packages_path()
            sys.path.append(os.path.join(site_packages_path, 'mapper'))
            try:
                from bsearch import mapfile_binary_search
                global log
                log = self.log_obj
                self.mapfile_binary_search = mapfile_binary_search
            except ImportError, e:
                self.fatal("Can't import mapfile_binary_search! %s\nDid you create-virtualenv?" % str(e))
        # I wish mapper did this for me, but ...
        fd = open(mapfile, 'rb')
        m = mmap.mmap(fd.fileno(), 0, mmap.MAP_PRIVATE, mmap.PROT_READ)
        return self.mapfile_binary_search(m, revision)

    def _post_fatal(self, message=None, exit_code=None):
        """ After we call fatal(), run this method before exiting.
            """
        if 'notify' in self.actions:
            self.notify(message=message, fatal=True)
        self.copy_logs_to_upload_dir()

    def _read_repo_update_json(self):
        """ repo_update.json is a file we create with information about each
            repo we're converting: git/hg branch names, git/hg revisions,
            pull datetime/timestamp, and push datetime/timestamp.

            Since we want to be able to incrementally update portions of this
            file as we pull/push each branch, we need to be able to read the
            json into memory, so we can update the dict and re-write the json
            to disk.
            """
        repo_map = {}
        dirs = self.query_abs_dirs()
        path = os.path.join(dirs['abs_upload_dir'], 'repo_update.json')
        if os.path.exists(path):
            fh = open(path, 'r')
            repo_map = json.load(fh)
            fh.close()
        return repo_map

    def query_abs_conversion_dir(self, repo_config):
        dirs = self.query_abs_dirs()
        if repo_config.get('conversion_dir'):
            dest = os.path.join(dirs['abs_work_dir'], 'conversion',
                                repo_config['conversion_dir'])
        else:
            dest = dirs.get('abs_conversion_dir')
        return dest

    def _write_repo_update_json(self, repo_map):
        """ The write portion of _read_repo_update_json().
            """
        dirs = self.query_abs_dirs()
        contents = json.dumps(repo_map, sort_keys=True, indent=4)
        self.write_to_file(
            os.path.join(dirs['abs_upload_dir'], 'repo_update.json'),
            contents,
            create_parent_dir=True
        )

    def _query_hg_exe(self):
        """Returns the hg executable command as a list
        """
        # If "hg" is set in "exes" section of config use that.
        # If not, get path from self.query_virtualenv_path() method
        # (respects --work-dir and --venv-path and --virtualenv-path).
        exe_command = self.query_exe('hg', return_type="list", default=[os.path.join(self.query_virtualenv_path(), "bin", "hg")])

        # possible additional command line options can be specified in "hg_options" of self.config
        hg_options = self.config.get("hg_options", ())
        exe_command.extend(hg_options)
        return exe_command

    def query_branches(self, branch_config, repo_path, vcs='hg'):
        """ Given a branch_config of branches and branch_regexes, return
            a dict of existing branch names to target branch names.
            """
        branch_map = {}
        if "branches" in branch_config:
            branch_map = deepcopy(branch_config['branches'])
        if "branch_regexes" in branch_config:
            regex_list = list(branch_config['branch_regexes'])
            full_branch_list = []
            if vcs == 'hg':
                hg = self._query_hg_exe()
                # This assumes we always want closed branches as well.
                # If not we may need more options.
                output = self.get_output_from_command(
                    hg + ['branches', '-a'],
                    cwd=repo_path
                )
                if output:
                    for line in output.splitlines():
                        full_branch_list.append(line.split()[0])
            elif vcs == 'git':
                git = self.query_exe("git", return_type="list")
                output = self.get_output_from_command(
                    git + ['branch', '-l'],
                    cwd=repo_path
                )
                if output:
                    for line in output.splitlines():
                        full_branch_list.append(line.replace('*', '').split()[0])
            for regex in regex_list:
                for branch in full_branch_list:
                    m = re.search(regex, branch)
                    if m:
                        # Don't overwrite branch_map[branch] if it exists
                        branch_map.setdefault(branch, branch)
        return branch_map

    def _combine_mapfiles(self, mapfiles, combined_mapfile, cwd=None):
        """ Adapted from repo-sync-tools/combine_mapfiles

            Consolidate multiple conversion processes' mapfiles into a
            single mapfile.
            """
        self.info("Determining whether we need to combine mapfiles...")
        if cwd is None:
            cwd = self.query_abs_dirs()['abs_upload_dir']
        existing_mapfiles = []
        for f in mapfiles:
            f_path = os.path.join(cwd, f)
            if os.path.exists(f_path):
                existing_mapfiles.append(f)
            else:
                self.warning("%s doesn't exist!" % f_path)
        combined_mapfile_path = os.path.join(cwd, combined_mapfile)
        if os.path.exists(combined_mapfile_path):
            combined_timestamp = os.path.getmtime(combined_mapfile_path)
            for f in existing_mapfiles:
                f_path = os.path.join(cwd, f)
                if os.path.getmtime(f_path) > combined_timestamp:
                    # Yes, we want to combine mapfiles
                    break
            else:
                self.info("No new mapfiles to combine.")
                return
            self.move(combined_mapfile_path, "%s.old" % combined_mapfile_path)
        output = self.get_output_from_command(
            ['sort', '--unique', '-t', ' ',
             '--key=2'] + existing_mapfiles,
            silent=True, halt_on_failure=True,
            cwd=cwd,
        )
        self.write_to_file(combined_mapfile_path, output, verbose=False,
                           error_level=FATAL)
        self.run_command(['ln', '-sf', combined_mapfile,
                          '%s-latest' % combined_mapfile],
                         cwd=cwd)

    # Actions {{{1

    def list_repos(self):
        repos = self.query_all_repos()
        self.info(pprint.pformat(repos))

    def create_test_targets(self):
        """ This action creates local directories to do test pushes to.
            """
        dirs = self.query_abs_dirs()
        for repo_config in self.query_all_non_failed_repos():
            for target_config in repo_config['targets']:
                if not target_config.get('test_push'):
                    continue
                target_dest = os.path.join(dirs['abs_target_dir'], target_config['target_dest'])
                if not os.path.exists(target_dest):
                    self.info("Creating local target repo %s." % target_dest)
                    if target_config.get("vcs", "git") == "git":
                        self.init_git_repo(target_dest, additional_args=['--bare', '--shared=all'],
                                           deny_deletes=True)
                    else:
                        self.fatal("Don't know how to deal with vcs %s!" % target_config['vcs'])
                else:
                    self.debug("%s exists; skipping." % target_dest)

    def update_stage_mirror(self):
        """ The stage mirror is a buffer clean clone of repositories.
            The logic behind this is that we get occasional corruption from
            |hg pull|.  It's much less time-consuming to detect this in
            a clean clone, and reclone, than to detect this in a working
            conversion directory, and try to repair or reclone+reconvert.

            We pull the stage mirror into the work mirror, where the conversion
            is done.
            """
        for repo_config in self.query_all_non_failed_repos():
            self._update_stage_repo(repo_config)

    def pull_out_new_sha_lookups(self, old_file, new_file):
        """ This method will return an iterator which iterates through lines in file
            new_file that do not exist in old_file. If old_file can't be read, all
            lines in new_file are returned. It does not cause any problems if lines
            exist in old_file that do not exist in new_file. Results are sorted by
            the second field (text after first space in line).

            This is somewhat equivalent to:
               ( [ ! -f "${old_file}" ] && cat "${new_file}" || diff "${old_file}" "${new_file}" | sed -n 's/> //p' ) | sort -k2"""
        with self.opened(old_file) as (old, err):
            if err:
                self.info('Map file %s not found - probably first time this has run.' % old_file)
                old_set = frozenset()
            else:
                old_set = frozenset(old)
        with self.opened(new_file, 'rt') as (new, err):
            if err:
                self.error('Could not read contents of map file %s:\n%s' % (new_file, err.message))
                new_set = frozenset()
            else:
                new_set = frozenset(new)
        for line in sorted(new_set.difference(old_set), key=lambda line: line.partition(' ')[2]):
            yield line

    def update_work_mirror(self):
        """ Pull the latest changes into the work mirror, update the repo_map
            json, and run |hg gexport| to convert those latest changes into
            the git conversion repo.
            """
        hg = self._query_hg_exe()
        git = self.query_exe("git", return_type="list")
        dirs = self.query_abs_dirs()
        repo_map = self._read_repo_update_json()
        timestamp = int(time.time())
        datetime = time.strftime('%Y-%m-%d %H:%M %Z')
        repo_map['last_pull_timestamp'] = timestamp
        repo_map['last_pull_datetime'] = datetime
        for repo_config in self.query_all_non_failed_repos():
            repo_name = repo_config['repo_name']
            source = os.path.join(dirs['abs_source_dir'], repo_name)
            dest = self.query_abs_conversion_dir(repo_config)
            if not dest:
                self.fatal("No conversion_dir for %s!" % repo_name)
            if not os.path.exists(dest):
                self.mkdir_p(os.path.dirname(dest))
                self.run_command(hg + ['clone', '--noupdate', source, dest],
                                 error_list=HgErrorList,
                                 halt_on_failure=False)
                if os.path.exists(dest):
                    self.write_hggit_hgrc(dest)
                    self.init_git_repo('%s/.git' % dest, additional_args=['--bare'])
                    self.run_command(
                    git + ['--git-dir', '%s/.git' % dest, 'config', 'gc.auto', '0'],
                    )
                else:
                    self.add_failure(
                        repo_name,
                        message="Failed to clone %s!" % source,
                        level=ERROR,
                    )
                    continue
            # Build branch map.
            branch_map = self.query_branches(
                repo_config.get('branch_config', {}),
                source,
            )
            for (branch, target_branch) in branch_map.items():
                output = self.get_output_from_command(
                    hg + ['id', '-r', branch],
                    cwd=source
                )
                if output:
                    rev = output.split(' ')[0]
                else:
                    self.add_failure(
                        repo_name,
                        message="Branch %s doesn't exist in %s (%s cloned into staging directory %s)!" % (branch, repo_name, repo_config.get('repo'), source),
                        level=ERROR,
                    )
                    continue
                timestamp = int(time.time())
                datetime = time.strftime('%Y-%m-%d %H:%M %Z')
                if self.run_command(hg + ['pull', '-r', rev, source], cwd=dest,
                                    error_list=HgErrorList):
                    # We shouldn't have an issue pulling!
                    self.add_failure(
                        repo_name,
                        message="Unable to pull %s from stage_source; clobbering and skipping!" % repo_name,
                        level=ERROR,
                    )
                    self._update_repo_previous_status(repo_name, successful_flag=False, write_update=True)
                    # don't leave a dirty checkout behind, and skip remaining branches
                    self.rmtree(source)
                    break
                self.run_command(
                    hg + ['bookmark', '-f', '-r', rev, target_branch],
                    cwd=dest, error_list=HgErrorList,
                )
                # This might get a little large.
                repo_map.setdefault('repos', {}).setdefault(repo_name, {}).setdefault('branches', {})[branch] = {
                    'hg_branch': branch,
                    'hg_revision': rev,
                    'git_branch': target_branch,
                    'pull_timestamp': timestamp,
                    'pull_datetime': datetime,
                }
            if self.query_failure(repo_name):
                # We hit an error in the for loop above
                continue
            self.retry(
                self.run_command,
                args=(hg + ['-v', 'gexport'], ),
                kwargs={
                    'output_timeout': 15 * 60,
                    'cwd': dest,
                    'error_list': HgErrorList,
                },
                error_level=FATAL,
            )
            generated_mapfile = os.path.join(dest, '.hg', 'git-mapfile')
            self.copy_to_upload_dir(
                generated_mapfile,
                dest=repo_config.get('mapfile_name', self.config.get('mapfile_name', "gecko-mapfile")),
                log_level=INFO
            )
            for (branch, target_branch) in branch_map.items():
                git_revision = self._query_mapped_revision(
                    revision=rev, mapfile=generated_mapfile)
                repo_map['repos'][repo_name]['branches'][branch]['git_revision'] = git_revision
        self._write_repo_update_json(repo_map)


    def create_git_notes(self):
        git = self.query_exe("git", return_type="list")
        for repo_config in self.query_all_non_failed_repos():
            repo = repo_config['repo']
            if repo_config.get('generate_git_notes', False):
                dest = self.query_abs_conversion_dir(repo_config)
                # 'git-mapfile' is created by hggit plugin, containing all the mappings
                complete_mapfile = os.path.join(dest, '.hg', 'git-mapfile')
                # 'added-to-git-notes' is the set of mappings known to be recorded in the git notes
                # of the project (typically 'git-mapfile' from previous run)
                added_to_git_notes = os.path.join(dest, '.hg', 'added-to-git-notes')
                # 'delta-git-notes' is the set of new mappings found on this iteration, that
                # now need to be added to the git notes of the project (the diff between the
                # previous two files described)
                delta_git_notes = os.path.join(dest, '.hg', 'delta-git-notes')
                git_dir = os.path.join(dest, '.git')
                self.rmtree(delta_git_notes)
                git_notes_adding_successful = True
                with self.opened(delta_git_notes, open_mode='w') as (delta_out, err):
                    if err:
                        git_notes_adding_successful = False
                        self.warn("Could not write list of unprocessed git note mappings to file %s - not critical" % delta_git_notes)
                    else:
                        for sha_lookup in self.pull_out_new_sha_lookups(added_to_git_notes, complete_mapfile):
                            print >>delta_out, sha_lookup,
                            (git_sha, hg_sha) = sha_lookup.split()
                            # only add git note if not already there - note
                            # devs may have added their own notes, so don't
                            # replace any existing notes, just add to them
                            output = self.get_output_from_command(
                                git + ['notes', 'show', git_sha],
                                cwd=git_dir,
                                ignore_errors=True
                            )
                            git_note_text='Upstream source: %s/rev/%s' % (repo, hg_sha)
                            git_notes_add_return_code = 1
                            if not output or output.find(git_note_text) < 0:
                                git_notes_add_return_code = self.run_command(
                                    git + ['notes', 'append', '-m', git_note_text, git_sha],
                                    cwd=git_dir
                                )
                            # if note was successfully added, or it was already there, we can
                            # mark it as added, by putting it in the delta file...
                            if git_notes_add_return_code == 0 or output.find(git_note_text) >= 0:
                                print >>delta_out, sha_lookup,
                            else:
                                self.error("Was not able to append required git note for git commit %s ('%s')" % (git_sha, git_note_text))
                                git_notes_adding_successful = False
                if git_notes_adding_successful:
                    self.copyfile(complete_mapfile, added_to_git_notes)
            else:
                self.info("Not creating git notes for repo %s (generate_git_notes not set to True)" % repo)

    def publish_to_mapper(self):
        """ This method will attempt to create git notes for any new git<->hg mappings
            found in the generated_mapfile file and also push new mappings to mapper service."""
        for repo_config in self.query_all_non_failed_repos():
            dest = self.query_abs_conversion_dir(repo_config)
            # 'git-mapfile' is created by hggit plugin, containing all the mappings
            complete_mapfile = os.path.join(dest, '.hg', 'git-mapfile')
            # 'published-to-mapper' is all the mappings that are known to be published
            # to mapper, for this project (typically the 'git-mapfile' from the previous
            # run)
            published_to_mapper = os.path.join(dest, '.hg', 'published-to-mapper')
            # 'delta-for-mapper' is the set of mappings that need to be published to
            # mapper on this iteration, i.e. the diff between the previous two files
            # described
            delta_for_mapper = os.path.join(dest, '.hg', 'delta-for-mapper')
            self.rmtree(delta_for_mapper)
            # we only replace published_to_mapper if we successfully updated
            # pushed to mapper
            mapper_config = repo_config.get('mapper', {})
            if mapper_config:
                site_packages_path = self.query_python_site_packages_path()
                if site_packages_path not in sys.path:
                    sys.path.append(site_packages_path)
                try:
                    import requests
                except ImportError as e:
                    self.error("Can't import requests: %s\nDid you create-virtualenv?" % str(e))
                mapper_url = mapper_config['url']
                mapper_project = mapper_config['project']
                insert_url = "%s/%s/insert/ignoredups" % (mapper_url, mapper_project)
                headers = {
                    'Content-Type': 'text/plain',
                    'Authentication': 'Bearer %s' % os.environ["RELENGAPI_INSERT_HGGIT_MAPPINGS_AUTH_TOKEN"]
                }
                all_new_mappings = []
                all_new_mappings.extend(self.pull_out_new_sha_lookups(published_to_mapper, complete_mapfile))
                self.write_to_file(delta_for_mapper, "".join(all_new_mappings))
                # due to timeouts on load balancer, we only push 200 lines at a time
                # this means that we should get http response back within 30 seconds
                # including the time it takes to insert the mappings in the database
                publish_successful = True
                for i in range(0, len(all_new_mappings), 200):
                    r = requests.post(insert_url, data="".join(all_new_mappings[i:i+200]), headers=headers)
                    if (r.status_code != 200):
                        self.error("Could not publish mapfile ('%s') line range [%s, %s] to mapper (%s) - received http %s code" % (delta_for_mapper, i, i+200, insert_url, r.status_code))
                        publish_successful = False
                        # we won't break out, since we may be able to publish other mappings
                        # and duplicates are allowed, so we will push the whole lot again next
                        # time anyway
                    else:
                        self.info("Published mapfile ('%s') line range [%s, %s] to mapper (%s)" % (delta_for_mapper, i, i+200, insert_url))
                if publish_successful:
                    # if we get this far, we know we could successfully post to mapper, so now
                    # we can copy the mapfile over "previously generated" version
                    # so that we don't push to mapper for these commits again
                    self.copyfile(complete_mapfile, published_to_mapper)
            else:
                self.copyfile(complete_mapfile, published_to_mapper)

    def combine_mapfiles(self):
        """ This method is for any job (l10n, project-branches) that needs to combine
            mapfiles.
            """
        if not self.config.get("combined_mapfile"):
            self.info("No combined_mapfile set in config; skipping!")
            return
        dirs = self.query_abs_dirs()
        mapfiles = []
        if self.config.get('conversion_type') == 'b2g-l10n':
            for repo_config in self.query_all_non_failed_repos():
                if repo_config.get("mapfile_name"):
                    mapfiles.append(repo_config['mapfile_name'])
        else:
            mapfiles.append(self.config.get('mapfile_name', 'gecko-mapfile'))
        if self.config.get('external_mapfile_urls'):
            for url in self.config['external_mapfile_urls']:
                file_name = self.download_file(
                    url,
                    parent_dir=dirs['abs_upload_dir'],
                    error_level=FATAL,
                )
                mapfiles.append(file_name)
        if not mapfiles:
            self.info("No mapfiles to combine; skipping!")
            return
        self._combine_mapfiles(mapfiles, self.config['combined_mapfile'])

    def push(self):
        """ Push to all targets.  test_targets are local directory test repos;
            the rest are remote.  Updates the repo_map json.
            """
        self.create_test_targets()
        repo_map = self._read_repo_update_json()
        failure_msg = ""
        timestamp = int(time.time())
        datetime = time.strftime('%Y-%m-%d %H:%M %Z')
        repo_map['last_push_timestamp'] = timestamp
        repo_map['last_push_datetime'] = datetime
        for repo_config in self.query_all_non_failed_repos():
            timestamp = int(time.time())
            datetime = time.strftime('%Y-%m-%d %H:%M %Z')
            status = self._push_repo(repo_config)
            repo_name = repo_config['repo_name']
            if not status:  # good
                if repo_name not in self.successful_repos:
                    self.successful_repos.append(repo_name)
                repo_map.setdefault('repos', {}).setdefault(repo_name, {})['push_timestamp'] = timestamp
                repo_map['repos'][repo_name]['push_datetime'] = datetime
                previous_status = self._query_repo_previous_status(repo_name, repo_map=repo_map)
                if previous_status is None:
                    self.add_summary("Possibly the first successful push of %s." % repo_name)
                elif previous_status is False:
                    self.add_summary("Previously unsuccessful push of %s is now successful!" % repo_name)
                self._update_repo_previous_status(repo_name, successful_flag=True, repo_map=repo_map, write_update=True)
            else:
                self.add_failure(
                    repo_name,
                    message="Unable to push %s." % repo_name,
                    level=ERROR,
                )
                failure_msg += status + "\n"
                self._update_repo_previous_status(repo_name, successful_flag=False, repo_map=repo_map, write_update=True)
        if not failure_msg:
            repo_map['last_successful_push_timestamp'] = repo_map['last_push_timestamp']
            repo_map['last_successful_push_datetime'] = repo_map['last_push_datetime']
        self._write_repo_update_json(repo_map)
        if failure_msg:
            self.fatal("Unable to push these repos:\n%s" % failure_msg)

    def preflight_upload(self):
        if not self.config.get("copy_logs_post_run", True):
            self.copy_logs_to_upload_dir()

    def upload(self):
        """ Upload the upload_dir according to the upload_config.
            """
        failure_msg = ''
        dirs = self.query_abs_dirs()
        for upload_config in self.config.get('upload_config', []):
            if self.retry(
                self.rsync_upload_directory,
                args=(
                    dirs['abs_upload_dir'],
                ),
                kwargs=upload_config,
            ):
                failure_msg += '%s:%s' % (upload_config['remote_host'],
                                          upload_config['remote_path'])
        if failure_msg:
            self.fatal("Unable to upload to this location:\n%s" % failure_msg)


# __main__ {{{1
if __name__ == '__main__':
    conversion = HgGitScript()
    conversion.run()
