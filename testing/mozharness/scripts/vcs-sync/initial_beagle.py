#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""initial_beagle.py

Multi-repo m-c hg->git initial conversion with cvs prepending, specifically for
gecko.git and beagle support.

Separated from hg_git.py for a) simplifying hg_git.py for its main purpose,
and b) somewhat protecting the initial conversion steps from future edits.
"""

from copy import deepcopy
import mmap
import os
import re
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

import mozharness
external_tools_path = os.path.join(
    os.path.abspath(os.path.dirname(os.path.dirname(mozharness.__file__))),
    'external_tools',
)

from mozharness.base.errors import HgErrorList, GitErrorList, TarErrorList
from mozharness.base.log import INFO, FATAL
from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSScript
from mozharness.mozilla.tooltool import TooltoolMixin


# HgGitScript {{{1
class HgGitScript(VirtualenvMixin, TooltoolMixin, TransferMixin, VCSScript):
    """ Beagle-oriented hg->git script (lots of mozilla-central hardcodes;
        assumption that we're going to be importing lots of branches).

        Beagle is a git repo of mozilla-central, with full cvs history,
        and a number of developer-oriented repositories and branches added.

        The partner-oriented gecko.git could also be incorporated into this
        script with some changes.
        """

    mapfile_binary_search = None
    successful_repos = []  # Unused; for notify() capability with vcs_sync.py

    def __init__(self, require_config_file=True):
        super(HgGitScript, self).__init__(
            config_options=virtualenv_config_options,
            all_actions=[
                'clobber',
                'create-virtualenv',
                'pull',
                'create-stage-mirror',
                'create-work-mirror',
                'initial-conversion',
                'prepend-cvs',
                'fix-tags',
                'notify',
            ],
            # These default actions are the update loop that we run after the
            # initial steps to create the work mirror with all the branches +
            # cvs history have been run.
            require_config_file=require_config_file
        )

    # Helper methods {{{1
    def query_abs_dirs(self):
        """ Define paths.
            """
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(HgGitScript, self).query_abs_dirs()
        abs_dirs['abs_cvs_history_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'mozilla-cvs-history')
        abs_dirs['abs_conversion_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'conversion',
            self.config['conversion_dir']
        )
        abs_dirs['abs_source_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'stage_source')
        abs_dirs['abs_repo_sync_tools_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'repo-sync-tools')
        abs_dirs['abs_git_rewrite_dir'] = os.path.join(
            abs_dirs['abs_work_dir'], 'mc-git-rewrite')
        abs_dirs['abs_target_dir'] = os.path.join(abs_dirs['abs_work_dir'],
                                                  'target')
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_all_repos(self):
        """ Very simple method, but we need this concatenated list many times
            throughout the script.
            """
        return [self.config['initial_repo']]

    def _update_stage_repo(self, repo_config, retry=True, clobber=False):
        """ Update a stage repo.
            The stage mirror is a buffer clean clone of repositories.
            The logic behind this is that we get occasional corruption from
            |hg pull|.  It's much less time-consuming to detect this in
            a clean clone, and reclone, than to detect this in a working
            conversion directory, and try to repair or reclone+reconvert.

            We pull the stage mirror into the work mirror, where the
            conversion
            is done.
            """
        hg = self.query_exe('hg', return_type='list')
        dirs = self.query_abs_dirs()
        source_dest = os.path.join(dirs['abs_source_dir'],
                                   repo_config['repo_name'])
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
                    self.fatal("Can't clone %s!" % repo_config['repo'])
        cmd = hg + ['pull']
        if self.retry(
            self.run_command,
            args=(cmd, ),
            kwargs={
                'output_timeout': 15 * 60,
                'cwd': source_dest,
            },
        ):
            if retry:
                return self._update_stage_repo(
                    repo_config, retry=False, clobber=True)
            else:
                self.fatal("Can't pull %s!" % repo_config['repo'])
        # commenting out hg verify since it takes ~5min per repo; hopefully
        # exit codes will save us
#        if self.run_command(hg + ["verify"], cwd=source_dest):
#            if retry:
#                return self._update_stage_repo(repo_config, retry=False, clobber=True)
#            else:
#                self.fatal("Can't verify %s!" % source_dest)

    def _check_initial_git_revisions(self, repo_path, expected_sha1,
                                     expected_sha2):
        """ Verify that specific git revisions match expected shas.

            This involves some hardcodes, which is unfortunate, but they save
            time, especially since we're hardcoding mozilla-central behavior
            anyway.
            """
        git = self.query_exe('git', return_type='list')
        output = self.get_output_from_command(
            git + ['log', '--oneline', '--grep', '374866'],
            cwd=repo_path
        )
        # hardcode test
        if not output:
            self.fatal("No output from git log!")
        rev = output.split(' ')[0]
        if not rev.startswith(expected_sha1):
            self.fatal("Output doesn't match expected sha %s for initial hg commit: %s" % (expected_sha1, str(output)))
        output = self.get_output_from_command(
            git + ['log', '-n', '1', '%s^' % rev],
            cwd=repo_path
        )
        if not output:
            self.fatal("No output from git log!")
        rev = output.splitlines()[0].split(' ')[1]
        if rev != expected_sha2:
            self.fatal("Output rev %s doesn't show expected rev %s:\n\n%s" % (rev, expected_sha2, output))

    def munge_mapfile(self):
        """ From https://github.com/ehsan/mozilla-history-tools/blob/master/initial_conversion/translate_git-mapfile.py
            """
        self.info("Updating pre-cvs mapfile...")
        dirs = self.query_abs_dirs()
        orig_mapfile = os.path.join(dirs['abs_upload_dir'], 'pre-cvs-mapfile')
        conversion_dir = dirs['abs_conversion_dir']
        mapfile = os.path.join(dirs['abs_work_dir'], 'post-cvs-mapfile')
        mapdir = os.path.join(dirs['abs_git_rewrite_dir'], 'map')
        orig_mapfile_fh = open(orig_mapfile, "r")
        mapfile_fh = open(mapfile, "w")
        for line in orig_mapfile_fh:
            tokens = line.split(" ")
            if len(tokens) == 2:
                git_sha = tokens[0].strip()
                hg_sha = tokens[1].strip()
                new_path = os.path.join(mapdir, git_sha)
                if os.path.exists(new_path):
                    translated_git_sha = open(new_path).read().strip()
                    print >>mapfile_fh, "%s %s" % (translated_git_sha, hg_sha)
                else:
                    print >>mapfile_fh, "%s %s" % (git_sha, hg_sha)
        orig_mapfile_fh.close()
        mapfile_fh.close()
        self.copyfile(
            mapfile,
            os.path.join(conversion_dir, '.hg', 'git-mapfile'),
            error_level=FATAL,
        )
        self.copy_to_upload_dir(mapfile, dest="post-cvs-mapfile",
                                log_level=INFO)

    def make_repo_bare(self, path, tmpdir=None):
        """ Since we do a |git checkout| in prepend_cvs(), and later want
            a bare repo.
            """
        self.info("Making %s/.git a bare repo..." % path)
        for p in (path, os.path.join(path, ".git")):
            if not os.path.exists(p):
                self.error("%s doesn't exist! Skipping..." % p)
        if tmpdir is None:
            tmpdir = os.path.dirname(os.path.abspath(path))
        git = self.query_exe("git", return_type="list")
        for dirname in (".git", ".hg"):
            if os.path.exists(os.path.join(path, dirname)):
                self.move(
                    os.path.join(path, dirname),
                    os.path.join(tmpdir, dirname),
                    error_level=FATAL,
                )
        self.rmtree(path, error_level=FATAL)
        self.mkdir_p(path)
        for dirname in (".git", ".hg"):
            if os.path.exists(os.path.join(tmpdir, dirname)):
                self.move(
                    os.path.join(tmpdir, dirname),
                    os.path.join(path, dirname),
                    error_level=FATAL,
                )
        self.run_command(
            git + ['--git-dir', os.path.join(path, ".git"),
                   'config', '--bool', 'core.bare', 'true'],
            halt_on_failure=True,
        )

    def _fix_tags(self, conversion_dir, git_rewrite_dir):
        """ Ehsan's git tag fixer, ported from bash.

         `` Git's history rewriting is not smart about preserving the tags in
            your repository, so you would end up with tags which point to
            commits in the old history line. If you push your repository to
            some other repository for example, all of the tags in the target
            repository would be invalid, since they would be pointing to
            commits that don't exist in that repository. ''

            https://github.com/ehsan/mozilla-history-tools/blob/master/initial_conversion/translate_git_tags.sh
            """
        self.info("Fixing tags...")
        git = self.query_exe('git', return_type='list')
        output = self.get_output_from_command(
            git + ['for-each-ref'],
            cwd=conversion_dir,
            halt_on_failure=True,
        )
        for line in output.splitlines():
            old_sha1, the_rest = line.split(' ')
            git_type, name = the_rest.split('	')
            if git_type == 'commit' and name.startswith('refs/tags'):
                path = os.path.join(git_rewrite_dir, 'map', old_sha1)
                if os.path.exists(path):
                    new_sha1 = self.read_from_file(path).rstrip()
                    self.run_command(
                        git + ['update-ref', name,
                               new_sha1, old_sha1],
                        cwd=conversion_dir,
                        error_list=GitErrorList,
                        halt_on_failure=True,
                    )

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
        conversion_dir = dirs['abs_conversion_dir']
        source_dir = os.path.join(dirs['abs_source_dir'], repo_config['repo_name'])
        git = self.query_exe('git', return_type='list')
        hg = self.query_exe('hg', return_type='list')
        return_status = ''
        for target_config in repo_config['targets']:
            if target_config.get("vcs", "git") == "git":
                base_command = git + ['push']
                env = {}
                if target_config.get("force_push"):
                    base_command.append("-f")
                if target_config.get("test_push"):
                    target_name = os.path.join(
                        dirs['abs_target_dir'], target_config['target_dest'])
                    base_command.append(target_name)
                else:
                    target_name = target_config['target_dest']
                    remote_config = self.config.get('remote_targets', {}).get(target_name)
                    if not remote_config:
                        self.fatal("Can't find %s in remote_targets!" % target_name)
                    base_command.append(remote_config['repo'])
                    # Allow for using a custom git ssh key.
                    env['GIT_SSH_KEY'] = remote_config['ssh_key']
                    env['GIT_SSH'] = os.path.join(external_tools_path, 'git-ssh-wrapper.sh')
                # Allow for pushing a subset of repo branches to the target.
                # If we specify that subset, we can also specify different
                # names for those branches (e.g. b2g18 -> master for a
                # standalone b2g18 repo)
                # We query hg for these because the conversion dir will have
                # branches from multiple hg repos, and the regexes may match
                # too many things.
                refs_list = []
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
                    for tag_line in tag_list.splitlines():
                        if not tag_line:
                            continue
                        tag_parts = tag_line.split()
                        if not tag_parts:
                            self.warning("Bogus tag_line? %s" % str(tag_line))
                            continue
                        tag_name = tag_parts[0]
                        for regex in regex_list:
                            if regex.search(tag_name) is not None:
                                refs_list += ['+refs/tags/%s:refs/tags/%s' % (tag_name, tag_name)]
                                continue
                error_msg = "%s: Can't push %s to %s!\n" % (repo_config['repo_name'], conversion_dir, target_name)
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
                    target_config['target_dest'], target_config['vcs'])
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
                hg = self.query_exe("hg", return_type="list")
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

    # Actions {{{1
    def create_stage_mirror(self):
        """ Rather than duplicate the logic here and in update_stage_mirror(),
            just call it.

            We could just create the initial_repo stage mirror here, but
            there's no real harm in cloning all repos here either.  Putting
            the time hit in the one-time-setup, rather than the first update
            loop, makes sense.
            """
        for repo_config in self.query_all_repos():
            self._update_stage_repo(repo_config)

    def write_hggit_hgrc(self, dest):
        # Update .hg/hgrc, if not already updated
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

    def create_work_mirror(self):
        """ Create the work_mirror, initial_repo only, from the stage_mirror.
            This is where the conversion will occur.
            """
        hg = self.query_exe("hg", return_type="list")
        git = self.query_exe("git", return_type="list")
        dirs = self.query_abs_dirs()
        repo_config = deepcopy(self.config['initial_repo'])
        work_dest = dirs['abs_conversion_dir']
        source_dest = os.path.join(
            dirs['abs_source_dir'], repo_config['repo_name'])
        if not os.path.exists(work_dest):
            self.run_command(hg + ["init", work_dest],
                             halt_on_failure=True)
        self.run_command(hg + ["pull", source_dest],
                         cwd=work_dest,
                         error_list=HgErrorList,
                         halt_on_failure=True)
        # The revision 82e4f1b7bbb6e30a635b49bf2107b41a8c26e3d2
        # reacts poorly to git-filter-branch-keep-rewrites (in
        # prepend-cvs), resulting in diverging shas.
        # To avoid this, strip back to 317fe0f314ab so the initial conversion
        # doesn't include 82e4f1b7bbb6e30a635b49bf2107b41a8c26e3d2, and
        # git-filter-branch-keep-rewrites is never run against this
        # revision.  This takes 3 strips, due to forking/merging.
        # See https://bugzilla.mozilla.org/show_bug.cgi?id=847727#c40 through
        # https://bugzilla.mozilla.org/show_bug.cgi?id=847727#c60
        # Also, yay hardcodes!
        for hg_revision in ("26cb30a532a1", "aad29aa89237", "9f2fa4839e98", "f8d0784186b7"):
            self.run_command(hg + ["--config", "extensions.mq=", "strip",
                                   "--no-backup", hg_revision],
                             cwd=work_dest,
                             error_list=HgErrorList,
                             halt_on_failure=True)
        # Make sure 317fe0f314ab is the only head!
        self.info("Making sure we've stripped m-c down to a single head 317fe0f314ab...")
        output = self.get_output_from_command(hg + ["heads"],
                                              cwd=work_dest,
                                              halt_on_failure=True)
        for line in output.splitlines():
            if line.startswith("changeset:") and not line.endswith("317fe0f314ab"):
                self.fatal("Found a head that is not 317fe0f314ab!  hg strip or <h<surkov will show up again!\n\n%s" % output)
        # Create .git for conversion, if it doesn't exist
        git_dir = os.path.join(work_dest, '.git')
        if not os.path.exists(git_dir):
            self.run_command(git + ['init'], cwd=work_dest)
            self.run_command(
                git + ['--git-dir', git_dir, 'config', 'gc.auto', '0'],
                cwd=work_dest
            )
        self.write_hggit_hgrc(work_dest)

    def initial_conversion(self):
        """ Run the initial hg-git conversion of the work_mirror.
            This will create a git m-c repository without cvs history.
            """
        hg = self.query_exe("hg", return_type="list")
        dirs = self.query_abs_dirs()
        repo_config = deepcopy(self.config['initial_repo'])
        source = os.path.join(dirs['abs_source_dir'], repo_config['repo_name'])
        dest = dirs['abs_conversion_dir']
        # bookmark all the branches in the repo_config, potentially
        # renaming them.
        # This follows a slightly different workflow than elsewhere; I don't
        # want to fiddle with this logic more than I have to.
        for (branch, target_branch) in repo_config.get('branch_config', {}).get('branches', {}).items():
            output = self.get_output_from_command(
                hg + ['id', '-r', branch], cwd=dest)
            if output:
                rev = output.split(' ')[0]
            self.run_command(
                hg + ['bookmark', '-f', '-r', rev, target_branch],
                cwd=dest,
                error_list=HgErrorList,
                halt_on_failure=True,
            )
        # bookmark all the other branches, with the same name.
        output = self.get_output_from_command(hg + ['branches', '-c'], cwd=source)
        for line in output.splitlines():
            branch_name = line.split(' ')[0]
            if branch_name in repo_config.get('branch_config', {}).get('branches', {}):
                continue
            self.run_command(
                hg + ['bookmark', '-f', '-r', branch_name, branch_name],
                cwd=dest,
                error_list=HgErrorList,
                halt_on_failure=True,
            )
        # Do the conversion!  This will take a day or so.
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
        # Save the pre-cvs mapfile.
        self.copy_to_upload_dir(os.path.join(dest, '.hg', 'git-mapfile'),
                                dest="pre-cvs-mapfile", log_level=INFO)

    def prepend_cvs(self):
        """ Prepend converted CVS history to the converted git repository,
            then munge the branches with git-filter-branch-keep-rewrites
            to adjust the shas.

            This step can take on the order of a week of compute time.
            """
        dirs = self.query_abs_dirs()
        git = self.query_exe('git', return_type='list')
        conversion_dir = dirs['abs_conversion_dir']
        git_conversion_dir = os.path.join(conversion_dir, '.git')
        grafts_file = os.path.join(git_conversion_dir, 'info', 'grafts')
        map_dir = os.path.join(git_conversion_dir, '.git-rewrite', 'map')
        if not os.path.exists(dirs["abs_cvs_history_dir"]):
            # gd2 doesn't have access to tooltool :(
            #manifest_path = self.create_tooltool_manifest(self.config['cvs_manifest'])
            #if self.tooltool_fetch(manifest_path, output_dir=dirs['abs_work_dir']):
            #    self.fatal("Unable to download cvs history via tooltool!")
            # Temporary workaround
            self.copyfile(
                self.config['cvs_history_tarball'],
                os.path.join(dirs['abs_work_dir'], "mozilla-cvs-history.tar.bz2")
            )
            self.run_command(
                ["tar", "xjvf", "mozilla-cvs-history.tar.bz2"],
                cwd=dirs["abs_work_dir"],
                error_list=TarErrorList,
                halt_on_failure=True
            )
        # We need to git checkout, or git thinks we've removed all the files
        # without committing
        self.run_command(git + ["checkout"], cwd=conversion_dir)
        self.run_command(
            'cp ' + os.path.join(dirs['abs_cvs_history_dir'], 'objects',
                                 'pack', '*') + ' .',
            cwd=os.path.join(git_conversion_dir, 'objects', 'pack')
        )
        self._check_initial_git_revisions(dirs['abs_cvs_history_dir'], 'e230b03',
                                          '3ec464b55782fb94dbbb9b5784aac141f3e3ac01')
        self._check_initial_git_revisions(conversion_dir, '4b3fd9',
                                          '2514a423aca5d1273a842918589e44038d046a51')
        self.write_to_file(grafts_file,
                           '2514a423aca5d1273a842918589e44038d046a51 3ec464b55782fb94dbbb9b5784aac141f3e3ac01')
        # This script is modified from git-filter-branch from git.
        # https://people.mozilla.com/~hwine/tmp/vcs2vcs/notes.html#initial-conversion
        # We may need to update this script if we update git.
        # Currently, due to the way the script outputs (with \r but not \n),
        # mozharness doesn't output anything for a number of ours.  This
        # prevents using the output_timeout run_command() option (or, if we
        # did, it would have to be many hours long).
        env = deepcopy(self.config.get('env', {}))
        git_filter_branch = os.path.join(
            dirs['abs_repo_sync_tools_dir'],
            'git-filter-branch-keep-rewrites'
        )
        self.run_command(
            [git_filter_branch, '--',
             '3ec464b55782fb94dbbb9b5784aac141f3e3ac01..HEAD'],
            partial_env=env,
            cwd=conversion_dir,
            halt_on_failure=True
        )
        # The self.move() will break if this dir already exists.
        # This rmtree() could move up to a preflight_prepend_cvs() method, or
        # near the beginning, if desired.
        self.rmtree(dirs['abs_git_rewrite_dir'])
        self.move(os.path.join(conversion_dir, '.git-rewrite'),
                  dirs['abs_git_rewrite_dir'],
                  error_level=FATAL)
        self.make_repo_bare(conversion_dir)
        branch_list = self.get_output_from_command(
            git + ['branch'],
            cwd=git_conversion_dir,
        )
        for branch in branch_list.splitlines():
            if branch.startswith('*'):
                # specifically deal with |* master|
                continue
            branch = branch.strip()
            self.run_command(
                [git_filter_branch, '-f', '--',
                 '3ec464b55782fb94dbbb9b5784aac141f3e3ac01..%s' % branch],
                partial_env=env,
                cwd=git_conversion_dir,
                halt_on_failure=True
            )
            # tar backup after mid-prepend-cvs gd2 reboot in bug 894225.
            if os.path.exists(map_dir):
                if self.config.get("backup_dir"):
                    self.mkdir_p(self.config['backup_dir'])
                    self.run_command(
                        ["tar", "cjvf",
                         os.path.join(self.config["backup_dir"],
                                      "prepend-cvs-%s.tar.bz2" % branch),
                         map_dir],
                    )
                self.run_command(
                    ['rsync', '-azv', os.path.join(map_dir, '.'),
                     os.path.join(dirs['abs_git_rewrite_dir'], 'map', '.')],
                    halt_on_failure=True
                )
                self.rmtree(os.path.join(git_conversion_dir, '.git-rewrite'),
                            error_level=FATAL)
        self.rmtree(grafts_file, error_level=FATAL)
        self.munge_mapfile()

    def fix_tags(self):
        """ Fairly fast action that points each existing tag to the new
            cvs-prepended sha.

            Then we git gc to get rid of old shas.  This doesn't specifically
            belong in this action, though it's the right spot in the workflow.
            We have to gc to get rid of the <h<surkov email issue that
            git-filter-branch-keep-rewrites gets rid of in the new shas.
            """
        dirs = self.query_abs_dirs()
        git = self.query_exe("git", return_type="list")
        conversion_dir = dirs['abs_conversion_dir']
        self._fix_tags(
            os.path.join(conversion_dir, '.git'),
            dirs['abs_git_rewrite_dir']
        )
        self.run_command(
            git + ['gc', '--aggressive'],
            cwd=os.path.join(conversion_dir, '.git'),
            error_list=GitErrorList,
            halt_on_failure=True,
        )


# __main__ {{{1
if __name__ == '__main__':
    conversion = HgGitScript()
    conversion.run()
