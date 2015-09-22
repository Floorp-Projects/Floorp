#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" gecko_migration.py

Merge day script for gecko (mozilla-central -> mozilla-aurora,
mozilla-aurora -> mozilla-beta, mozilla-beta -> mozilla-release).

Ported largely from
http://hg.mozilla.org/build/tools/file/084bc4e2fc76/release/beta2release.py
and
http://hg.mozilla.org/build/tools/file/084bc4e2fc76/release/merge_helper.py
"""

from getpass import getpass
import os
import pprint
import subprocess
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.errors import HgErrorList
from mozharness.base.log import INFO, FATAL
from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.vcs.vcsbase import MercurialScript
from mozharness.base.vcs.mercurial import MercurialVCS
from mozharness.mozilla.selfserve import SelfServeMixin
from mozharness.mozilla.updates.balrog import BalrogMixin

VALID_MIGRATION_BEHAVIORS = (
    "beta_to_release", "aurora_to_beta", "central_to_aurora", "release_to_esr"
)


# GeckoMigration {{{1
class GeckoMigration(MercurialScript, BalrogMixin, VirtualenvMixin, SelfServeMixin):
    config_options = [
        [['--hg-user', ], {
            "action": "store",
            "dest": "hg_user",
            "type": "string",
            "default": "ffxbld <release@mozilla.com>",
            "help": "Specify what user to use to commit to hg.",
        }],
        [['--balrog-username', ], {
            "action": "store",
            "dest": "balrog_username",
            "type": "string",
            "default": "ffxbld",
            "help": "Specify what user to connect to Balrog with.",
        }],
        [['--balrog-credentials-file', ], {
            "action": "store",
            "dest": "balrog_credentials_file",
            "type": "string",
            "help": "The file containing the Balrog credentials.",
        }],
        [['--remove-locale', ], {
            "action": "extend",
            "dest": "remove_locales",
            "type": "string",
            "help": "Comma separated list of locales to remove from the 'to' repo.",
        }],
    ]
    gecko_repos = None

    def __init__(self, require_config_file=True):
        super(GeckoMigration, self).__init__(
            config_options=virtualenv_config_options + self.config_options,
            all_actions=[
                'clobber',
                'create-virtualenv',
                'clean-repos',
                'pull',
                'lock-update-paths',
                'migrate',
                'commit-changes',
                'push',
                'trigger-builders',
            ],
            default_actions=[
                'clean-repos',
                'pull',
                'migrate',
            ],
            require_config_file=require_config_file
        )
        self.run_sanity_check()

# Helper methods {{{1
    def run_sanity_check(self):
        """ Verify the configs look sane before proceeding.
            """
        message = ""
        if self.config['migration_behavior'] not in VALID_MIGRATION_BEHAVIORS:
            message += "%s must be one of %s!\n" % (self.config['migration_behavior'], VALID_MIGRATION_BEHAVIORS)
        if self.config['migration_behavior'] == 'beta_to_release':
            if self.config.get("require_remove_locales") and not self.config.get("remove_locales") and 'migrate' in self.actions:
                message += "You must specify --remove-locale!\n"
        else:
            if self.config.get("require_remove_locales") or self.config.get("remove_locales"):
                self.warning("--remove-locale isn't valid unless you're using beta_to_release migration_behavior!\n")
        if message:
            self.fatal(message)

    def query_abs_dirs(self):
        """ Allow for abs_from_dir and abs_to_dir
            """
        if self.abs_dirs:
            return self.abs_dirs
        dirs = super(GeckoMigration, self).query_abs_dirs()
        self.abs_dirs['abs_tools_dir'] = os.path.join(
            dirs['abs_work_dir'], 'tools'
        )
        self.abs_dirs['abs_tools_lib_dir'] = os.path.join(
            dirs['abs_work_dir'], 'tools', 'lib', 'python'
        )
        for k in ('from', 'to'):
            dir_name = self.config.get(
                "%s_repo_dir",
                self.get_filename_from_url(self.config["%s_repo_url" % k])
            )
            self.abs_dirs['abs_%s_dir' % k] = os.path.join(
                dirs['abs_work_dir'], dir_name
            )
        return self.abs_dirs

    def query_gecko_repos(self):
        """ Build a list of repos to clone.
            """
        if self.gecko_repos:
            return self.gecko_repos
        self.info("Building gecko_repos list...")
        dirs = self.query_abs_dirs()
        self.gecko_repos = []
        for k in ('from', 'to'):
            url = self.config["%s_repo_url" % k]
            self.gecko_repos.append({
                "repo": url,
                "revision": self.config.get("%s_repo_revision", "default"),
                "dest": dirs['abs_%s_dir' % k],
                "vcs": "hg",
            })
        self.info(pprint.pformat(self.gecko_repos))
        return self.gecko_repos

    def query_hg_revision(self, path):
        """ Avoid making 'pull' a required action every run, by being able
            to fall back to figuring out the revision from the cloned repo
            """
        m = MercurialVCS(log_obj=self.log_obj, config=self.config)
        revision = m.get_revision_from_path(path)
        return revision

    def query_from_revision(self):
        """ Shortcut to get the revision for the from repo
            """
        dirs = self.query_abs_dirs()
        return self.query_hg_revision(dirs['abs_from_dir'])

    def query_to_revision(self):
        """ Shortcut to get the revision for the to repo
            """
        dirs = self.query_abs_dirs()
        return self.query_hg_revision(dirs['abs_to_dir'])

    def get_fx_major_version(self, path):
        version_path = os.path.join(path, "browser", "config", "version.txt")
        contents = self.read_from_file(version_path, error_level=FATAL)
        return contents.split(".")[0]

    def hg_tag(self, cwd, tags, user=None, message=None, revision=None,
               force=None, halt_on_failure=True):
        if isinstance(tags, basestring):
            tags = [tags]
        message = "No bug - Tagging %s" % os.path.basename(cwd)
        if revision:
            message = "%s %s" % (message, revision)
        message = "%s with %s" % (message, ', '.join(tags))
        message += " a=release DONTBUILD CLOSED TREE"
        self.info(message)
        cmd = self.query_exe('hg', return_type='list') + ['tag']
        if user:
            cmd.extend(['-u', user])
        if message:
            cmd.extend(['-m', message])
        if revision:
            cmd.extend(['-r', revision])
        if force:
            cmd.append('-f')
        cmd.extend(tags)
        return self.run_command(
            cmd, cwd=cwd, halt_on_failure=halt_on_failure,
            error_list=HgErrorList
        )

    def hg_commit(self, cwd, message, user=None, ignore_no_changes=False):
        """ Commit changes to hg.
            """
        cmd = self.query_exe('hg', return_type='list') + [
            'commit', '-m', message]
        if user:
            cmd.extend(['-u', user])
        success_codes = [0]
        if ignore_no_changes:
            success_codes.append(1)
        self.run_command(
            cmd, cwd=cwd, error_list=HgErrorList,
            halt_on_failure=True,
            success_codes=success_codes
        )
        return self.query_hg_revision(cwd)

    def hg_merge_via_debugsetparents(self, cwd, old_head, new_head,
                                     preserve_tags=True, user=None):
        """ Merge 2 heads avoiding non-fastforward commits
            """
        hg = self.query_exe('hg', return_type='list')
        cmd = hg + ['debugsetparents', new_head, old_head]
        self.run_command(cmd, cwd=cwd, error_list=HgErrorList,
                         halt_on_failure=True)
        self.hg_commit(
            cwd,
            message="Merge old head via |hg debugsetparents %s %s|. "
            "CLOSED TREE DONTBUILD a=release" % (new_head, old_head),
            user=user
        )
        if preserve_tags:
            # I don't know how to do this elegantly.
            # I'm reverting .hgtags to old_head, then appending the new tags
            # from new_head to .hgtags, and hoping nothing goes wrong.
            # I'd rather not write patch files from scratch, so this seems
            # like a slightly more complex but less objectionable method?
            self.info("Trying to preserve tags from before debugsetparents...")
            dirs = self.query_abs_dirs()
            patch_file = os.path.join(dirs['abs_work_dir'], 'patch_file')
            self.run_command(
                subprocess.list2cmdline(hg + ['diff', '-r', old_head, '.hgtags', '-U9', '>', patch_file]),
                cwd=cwd,
            )
            self.run_command(
                ['patch', '-R', '-p1', '-i', patch_file],
                cwd=cwd,
                halt_on_failure=True,
            )
            tag_diff = self.read_from_file(patch_file)
            with self.opened(os.path.join(cwd, '.hgtags'), open_mode='a') as (fh, err):
                if err:
                    self.fatal("Can't append to .hgtags!")
                for n, line in enumerate(tag_diff.splitlines()):
                    # The first 4 lines of a patch are headers, so we ignore them.
                    if n < 5:
                        continue
                    # Even after that, the only lines we really care about are
                    # additions to the file.
                    # TODO: why do we only care about additions? I couldn't
                    # figure that out by reading this code.
                    if not line.startswith('+'):
                        continue
                    line = line.replace('+', '')
                    (changeset, tag) = line.split(' ')
                    if len(changeset) != 40:
                        continue
                    fh.write("%s\n" % line)
            out = self.get_output_from_command(['hg', 'status', '.hgtags'],
                                               cwd=cwd)
            if out:
                self.hg_commit(
                    cwd,
                    message="Preserve old tags after debugsetparents. "
                    "CLOSED TREE DONTBUILD a=release",
                    user=user,
                )
            else:
                self.info(".hgtags file is identical, no need to commit")

    def replace(self, file_name, from_, to_):
        """ Replace text in a file.
            """
        text = self.read_from_file(file_name, error_level=FATAL)
        new_text = text.replace(from_, to_)
        if text == new_text:
            self.fatal("Cannot replace '%s' to '%s' in '%s'" %
                       (from_, to_, file_name))
        self.write_to_file(file_name, new_text, error_level=FATAL)

    def remove_locales(self, file_name, locales):
        """ Remove locales from shipped-locales (m-r only)
            """
        contents = self.read_from_file(file_name)
        new_contents = ""
        for line in contents.splitlines():
            locale = line.split()[0]
            if locale not in locales:
                new_contents += "%s\n" % line
            else:
                self.info("Removed locale: %s" % locale)
        self.write_to_file(file_name, new_contents)

    def touch_clobber_file(self, cwd):
        clobber_file = os.path.join(cwd, 'CLOBBER')
        contents = self.read_from_file(clobber_file)
        new_contents = ""
        for line in contents.splitlines():
            line = line.strip()
            if line.startswith("#") or line == '':
                new_contents += "%s\n" % line
        new_contents += "Merge day clobber"
        self.write_to_file(clobber_file, new_contents)

    def bump_version(self, cwd, curr_version, next_version, curr_suffix,
                     next_suffix, bump_major=False):
        """ Bump versions (m-c, m-a, m-b).

            At some point we may want to unhardcode these filenames into config
            """
        curr_weave_version = str(int(curr_version) + 2)
        next_weave_version = str(int(curr_weave_version) + 1)
        for f in self.config["version_files"]:
            self.replace(os.path.join(cwd, f), "%s.0%s" % (curr_version, curr_suffix),
                         "%s.0%s" % (next_version, next_suffix))

        # only applicable for m-c
        if bump_major:
            self.replace(
                os.path.join(cwd, "xpcom/components/Module.h"),
                "static const unsigned int kVersion = %s;" % curr_version,
                "static const unsigned int kVersion = %s;" % next_version
            )
            self.replace(
                os.path.join(cwd, "services/sync/moz.build"),
                "DEFINES['weave_version'] = '1.%s.0'" % curr_weave_version,
                "DEFINES['weave_version'] = '1.%s.0'" % next_weave_version
            )

    # Branch-specific workflow helper methods {{{1
    def central_to_aurora(self, end_tag):
        """ mozilla-central -> mozilla-aurora behavior.

            We could have all of these individually toggled by flags, but
            by separating into workflow methods we can be more precise about
            what happens in each workflow, while allowing for things like
            staging beta user repo migrations.
            """
        dirs = self.query_abs_dirs()
        self.info("Reverting locales")
        hg = self.query_exe("hg", return_type="list")
        for f in self.config["locale_files"]:
            self.run_command(
                hg + ["revert", "-r", end_tag, f],
                cwd=dirs['abs_to_dir'],
                error_list=HgErrorList,
                halt_on_failure=True,
            )
        next_ma_version = self.get_fx_major_version(dirs['abs_to_dir'])
        self.bump_version(dirs['abs_to_dir'], next_ma_version, next_ma_version, "a1", "a2")
        self.apply_replacements()
        # bump m-c version
        curr_mc_version = self.get_fx_major_version(dirs['abs_from_dir'])
        next_mc_version = str(int(curr_mc_version) + 1)
        self.bump_version(
            dirs['abs_from_dir'], curr_mc_version, next_mc_version, "a1", "a1",
            bump_major=True
        )
        # touch clobber files
        self.touch_clobber_file(dirs['abs_from_dir'])
        self.touch_clobber_file(dirs['abs_to_dir'])

    def aurora_to_beta(self, *args, **kwargs):
        """ mozilla-aurora -> mozilla-beta behavior.

            We could have all of these individually toggled by flags, but
            by separating into workflow methods we can be more precise about
            what happens in each workflow, while allowing for things like
            staging beta user repo migrations.
            """
        dirs = self.query_abs_dirs()
        mb_version = self.get_fx_major_version(dirs['abs_to_dir'])
        self.bump_version(dirs['abs_to_dir'], mb_version, mb_version, "a2", "")
        self.apply_replacements()
        self.touch_clobber_file(dirs['abs_to_dir'])
        # TODO mozconfig diffing
        # The build/tools version only checks the mozconfigs from hgweb, so
        # can't help pre-push.  The in-tree mozconfig diffing requires a mach
        # virtualenv to be installed.  If we want this sooner we can put this
        # in the push action; otherwise we may just wait until we have in-tree
        # mozconfig checking.

    def beta_to_release(self, *args, **kwargs):
        """ mozilla-beta -> mozilla-release behavior.

            We could have all of these individually toggled by flags, but
            by separating into workflow methods we can be more precise about
            what happens in each workflow, while allowing for things like
            staging beta user repo migrations.
            """
        dirs = self.query_abs_dirs()
        self.apply_replacements()
        if self.config.get("remove_locales"):
            self.remove_locales(
                os.path.join(dirs['abs_to_dir'], "browser/locales/shipped-locales"),
                self.config['remove_locales']
            )
        self.touch_clobber_file(dirs['abs_to_dir'])

    def release_to_esr(self, *args, **kwargs):
        """ mozilla-release -> mozilla-esrNN behavior. """
        dirs = self.query_abs_dirs()
        for to_transplant in self.config.get("transplant_patches", []):
            self.transplant(repo=to_transplant["repo"],
                            changeset=to_transplant["changeset"],
                            cwd=dirs['abs_to_dir'])
        self.apply_replacements()
        self.touch_clobber_file(dirs['abs_to_dir'])

    def apply_replacements(self):
        dirs = self.query_abs_dirs()
        for f, from_, to in self.config["replacements"]:
            self.replace(os.path.join(dirs['abs_to_dir'], f), from_, to)

    def transplant(self, repo, changeset, cwd):
        """Transplant a Mercurial changeset from a remote repository."""
        hg = self.query_exe("hg", return_type="list")
        cmd = hg + ["--config", "extensions.transplant=", "transplant",
                    "--source", repo, changeset]
        self.info("Transplanting %s from %s" % (changeset, repo))
        status = self.run_command(
            cmd,
            cwd=cwd,
            error_list=HgErrorList,
        )
        if status != 0:
            self.fatal("Cannot transplant %s from %s properly" %
                       (changeset, repo))

    def pull_from_repo(self, from_dir, to_dir, revision=None, branch=None):
        """ Pull from one repo to another. """
        hg = self.query_exe("hg", return_type="list")
        cmd = hg + ["pull"]
        if revision:
            cmd.extend(["-r", revision])
        cmd.append(from_dir)
        self.run_command(
            cmd,
            cwd=to_dir,
            error_list=HgErrorList,
            halt_on_failure=True,
        )
        cmd = hg + ["update", "-C"]
        if branch or revision:
            cmd.extend(["-r", branch or revision])
        self.run_command(
            cmd,
            cwd=to_dir,
            error_list=HgErrorList,
            halt_on_failure=True,
        )

# Actions {{{1
    def clean_repos(self):
        """ We may end up with contaminated local repos at some point, but
            we don't want to have to clobber and reclone from scratch every
            time.

            This is an attempt to clean up the local repos without needing a
            clobber.
            """
        dirs = self.query_abs_dirs()
        hg = self.query_exe("hg", return_type="list")
        hg_repos = self.query_gecko_repos()
        hg_strip_error_list = [{
            'substr': r'''abort: empty revision set''', 'level': INFO,
            'explanation': "Nothing to clean up; we're good!",
        }] + HgErrorList
        for repo_config in hg_repos:
            repo_name = repo_config["dest"]
            repo_path = os.path.join(dirs['abs_work_dir'], repo_name)
            if os.path.exists(repo_path):
                # hg up -C to discard uncommitted changes
                self.run_command(
                    hg + ["up", "-C", "-r", repo_config['revision']],
                    cwd=repo_path,
                    error_list=HgErrorList,
                    halt_on_failure=True,
                )
                # discard unpushed commits
                status = self.retry(
                    self.run_command,
                    args=(hg + ["--config", "extensions.mq=", "strip",
                          "--no-backup", "outgoing()"], ),
                    kwargs={
                        'cwd': repo_path,
                        'error_list': hg_strip_error_list,
                        'return_type': 'num_errors',
                        'success_codes': (0, 255),
                    },
                )
                if status not in [0, 255]:
                    self.fatal("Issues stripping outgoing revisions!")
                # 2nd hg up -C to make sure we're not on a stranded head
                # which can happen when reverting debugsetparents
                self.run_command(
                    hg + ["up", "-C", "-r", repo_config['revision']],
                    cwd=repo_path,
                    error_list=HgErrorList,
                    halt_on_failure=True,
                )

    def pull(self):
        """ Pull tools first, then use hgtool for the gecko repos
            """
        repos = [{
            "repo": self.config["tools_repo_url"],
            "revision": self.config["tools_repo_revision"],
            "dest": "tools",
            "vcs": "hg",
        }] + self.query_gecko_repos()
        super(GeckoMigration, self).pull(repos=repos)

    def lock_update_paths(self):
        self.lock_balrog_rules(self.config["balrog_rules_to_lock"])

    def migrate(self):
        """ Perform the migration.
            """
        dirs = self.query_abs_dirs()
        from_fx_major_version = self.get_fx_major_version(dirs['abs_from_dir'])
        to_fx_major_version = self.get_fx_major_version(dirs['abs_to_dir'])
        base_from_rev = self.query_from_revision()
        base_to_rev = self.query_to_revision()
        base_tag = self.config['base_tag'] % {'major_version': from_fx_major_version}
        end_tag = self.config['end_tag'] % {'major_version': to_fx_major_version}
        self.hg_tag(
            dirs['abs_from_dir'], base_tag, user=self.config['hg_user'],
            message="Added %s tag for changeset %s. IGNORE BROKEN CHANGESETS DONTBUILD CLOSED TREE NO BUG a=release" %
                    (base_tag, base_from_rev),
            revision=base_from_rev,
        )
        new_from_rev = self.query_from_revision()
        self.info("New revision %s" % new_from_rev)
        pull_revision = None
        if not self.config.get("pull_all_branches"):
            pull_revision = new_from_rev
        self.pull_from_repo(
            dirs['abs_from_dir'], dirs['abs_to_dir'],
            revision=pull_revision,
            branch="default",
        )
        if self.config.get("requires_head_merge") is not False:
            self.hg_merge_via_debugsetparents(
                dirs['abs_to_dir'], old_head=base_to_rev, new_head=new_from_rev,
                user=self.config['hg_user'],
            )
        self.hg_tag(
            dirs['abs_to_dir'], end_tag, user=self.config['hg_user'],
            message="Added %s tag for changeset %s. IGNORE BROKEN CHANGESETS DONTBUILD CLOSED TREE NO BUG a=release" %
                    (end_tag, base_to_rev),
            revision=base_to_rev, force=True,
        )
        # Call beta_to_release etc.
        if not hasattr(self, self.config['migration_behavior']):
            self.fatal("Don't know how to proceed with migration_behavior %s !" % self.config['migration_behavior'])
        getattr(self, self.config['migration_behavior'])(end_tag=end_tag)
        self.info("Verify the diff, and apply any manual changes, such as disabling features, and --commit-changes")

    def commit_changes(self):
        """ Do the commit.
            """
        hg = self.query_exe("hg", return_type="list")
        dirs = self.query_abs_dirs()
        commit_dirs = [dirs['abs_to_dir']]
        if self.config['migration_behavior'] == 'central_to_aurora':
            commit_dirs.append(dirs['abs_from_dir'])
        for cwd in commit_dirs:
            self.run_command(hg + ["diff"], cwd=cwd)
            self.hg_commit(
                cwd, user=self.config['hg_user'],
                message="Update configs. IGNORE BROKEN CHANGESETS CLOSED TREE NO BUG a=release ba=release"
            )
        self.info("Now verify |hg out| and |hg out --patch| if you're paranoid, and --push")

    def push(self):
        """
            """
        error_message = """Push failed!  If there was a push race, try rerunning
the script (--clean-repos --pull --migrate).  The second run will be faster."""
        dirs = self.query_abs_dirs()
        hg = self.query_exe("hg", return_type="list")
        for cwd in (dirs['abs_from_dir'], dirs['abs_to_dir']):
            push_cmd = hg + ['push']
            if cwd == dirs['abs_to_dir'] and self.config['migration_behavior'] == 'beta_to_release':
                push_cmd.append('--new-branch')
            status = self.run_command(
                push_cmd,
                cwd=cwd,
                error_list=HgErrorList,
                success_codes=[0, 1],
            )
            if status == 1:
                self.warning("No changes for %s!" % cwd)
            elif status:
                if cwd == dirs['abs_from_dir'] and self.config['migration_behavior'] == 'central_to_aurora':
                    message = """m-c push failed!
You may be able to fix by |hg rebase| and rerunning --push if successful.
If not, try rerunning the script (--clean-repos --pull --migrate).
The second run will be faster."""
                else:
                    message = error_message
                self.fatal(message)

    def trigger_builders(self):
        """Triggers builders that should be run directly after a merge.
        There are two different types of things we trigger:
        1) Nightly builds ("post_merge_nightly_branches" in the config).
           These are triggered with buildapi's nightly build endpoint to avoid
           duplicating all of the nightly builder names into the gecko
           migration mozharness configs. (Which would surely get out of date
           very quickly).
        2) Arbitrary builders ("post_merge_builders"). These are additional
           builders to trigger that aren't part of the nightly builder set.
           For example: hg bundle generation builders.
        """
        dirs = self.query_abs_dirs()
        branch = self.config["to_repo_url"].rstrip("/").split("/")[-1]
        revision = self.query_to_revision()
        # Horrible hack because our internal buildapi interface doesn't let us
        # actually do anything. Need to use the public one w/ auth.
        username = raw_input("LDAP Username: ")
        password = getpass(prompt="LDAP Password: ")
        auth = (username, password)
        for builder in self.config["post_merge_builders"]:
            self.trigger_arbitrary_job(builder, branch, revision, auth)
        for nightly_branch in self.config["post_merge_nightly_branches"]:
            nightly_revision = self.query_hg_revision(os.path.join(dirs["abs_work_dir"], nightly_branch))
            self.trigger_nightly_builds(nightly_branch, nightly_revision, auth)

# __main__ {{{1
if __name__ == '__main__':
    gecko_migration = GeckoMigration()
    gecko_migration.run_and_exit()
