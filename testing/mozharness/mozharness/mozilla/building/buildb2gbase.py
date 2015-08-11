#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" buildb2gbase.py

provides a base class for b2g builds

"""
import os
import functools
import time
import random
import urlparse
import os.path
from external_tools.detect_repo import detect_git, detect_hg, detect_local

try:
    import simplejson as json
    assert json
except ImportError:
    import json

from mozharness.base.errors import MakefileErrorList
from mozharness.base.log import WARNING, ERROR, DEBUG
from mozharness.base.script import BaseScript
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.mock import MockMixin
from mozharness.mozilla.tooltool import TooltoolMixin
from mozharness.mozilla.repo_manifest import (load_manifest, rewrite_remotes,
                                              remove_project, get_project,
                                              map_remote, get_remote)

# B2G builds complain about java...but it doesn't seem to be a problem
# Let's turn those into WARNINGS instead
B2GMakefileErrorList = MakefileErrorList + [
    {'substr': r'''NS_ERROR_FILE_ALREADY_EXISTS: Component returned failure code''', 'level': ERROR},
    {'substr': r'''no version information available''', 'level': DEBUG},
]
B2GMakefileErrorList.insert(0, {'substr': r'/bin/bash: java: command not found', 'level': WARNING})


class B2GBuildBaseScript(BuildbotMixin, MockMixin,
                         BaseScript,
                         TooltoolMixin, VCSMixin, TransferMixin):
    default_config_options = [
        [["--repo"], {
            "dest": "repo",
            "help": "which gecko repo to check out",
        }],
        [["--target"], {
            "dest": "target",
            "help": "specify which build type to do",
        }],
        [["--b2g-config-dir"], {
            "dest": "b2g_config_dir",
            "help": "specify which in-tree config directory to use, relative to b2g/config/ (defaults to --target)",
        }],
        [["--gecko-config"], {
            "dest": "gecko_config",
            "help": "specfiy alternate location for gecko config",
        }],
        [["--disable-ccache"], {
            "dest": "ccache",
            "action": "store_false",
            "help": "disable ccache",
        }],
        [["--variant"], {
            "dest": "variant",
            "help": "b2g build variant. overrides gecko config's value",
        }],
        [["--checkout-revision"], {
            "dest": "checkout_revision",
            "help": "checkout a specific gecko revision.",
        }],
        [["--repotool-repo"], {
            "dest": "repo_repo",
            "help": "where to pull repo tool source from",
        }],
        [["--repotool-revision"], {
            "dest": "repo_rev",
            "help": "which revision of repo tool to use",
        }],
        [["--disable-mock"], {
            "dest": "disable_mock",
            "action": "store_true",
            "help": "do not run under mock despite what gecko-config says",
        }],
    ]

    def __init__(self,
                 config_options=[],
                 require_config_file=False,
                 config={},
                 all_actions=[],
                 default_actions=[]):
        default_config = {
            'vcs_share_base': os.environ.get('HG_SHARE_BASE_DIR'),
            'buildbot_json_path': os.environ.get('PROPERTIES_FILE'),
            'tools_repo': 'https://hg.mozilla.org/build/tools',
            'repo_repo': "https://git.mozilla.org/external/google/gerrit/git-repo.git",
            'repo_rev': 'stable',
            'hgurl': 'https://hg.mozilla.org/',
        }
        default_config.update(config)

        config_options = config_options + self.default_config_options

        super(B2GBuildBaseScript, self).__init__(
            config_options=config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions)

        self.gecko_config = None

    def _pre_config_lock(self, rw_config):
        super(B2GBuildBaseScript, self)._pre_config_lock(rw_config)

        if self.buildbot_config is None:
            self.info("Reading buildbot build properties...")
            self.read_buildbot_config()

        if 'target' not in self.config:
            self.fatal("Must specify --target!")

        # Override target for things with weird names
        if self.config['target'] == 'mako':
            self.info("Using target nexus-4 instead of mako")
            self.config['target'] = 'nexus-4'
            if self.config.get('b2g_config_dir') is None:
                self.config['b2g_config_dir'] = 'mako'
        elif self.config['target'] == 'generic':
            if self.config.get('b2g_config_dir') == 'emulator':
                self.info("Using target emulator instead of generic")
                self.config['target'] = 'emulator'
            elif self.config.get('b2g_config_dir') == 'emulator-jb':
                self.info("Using target emulator-jb instead of generic")
                self.config['target'] = 'emulator-jb'
            elif self.config.get('b2g_config_dir') == 'emulator-kk':
                self.info("Using target emulator-kk instead of generic")
                self.config['target'] = 'emulator-kk'

        if not (self.buildbot_config and 'properties' in self.buildbot_config) and 'repo' not in self.config:
            self.fatal("Must specify --repo")

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = BaseScript.query_abs_dirs(self)

        dirs = {
            'gecko_src': os.path.join(abs_dirs['abs_work_dir'], 'gecko'),
            'work_dir': abs_dirs['abs_work_dir'],
            'b2g_src': abs_dirs['abs_work_dir'],
            'abs_tools_dir': os.path.join(abs_dirs['abs_work_dir'], 'build-tools'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_repo(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.config['hgurl'] + self.buildbot_config['properties']['repo_path']
        else:
            return self.config['repo']

    def query_revision(self):
        if 'revision' in self.buildbot_properties:
            revision = self.buildbot_properties['revision']
        elif self.buildbot_config and 'sourcestamp' in self.buildbot_config:
            revision = self.buildbot_config['sourcestamp']['revision']
        else:
            dirs = self.query_abs_dirs()
            repo = dirs['gecko_src']
            repo_type = detect_local(repo)
            # Look at what we have checked out
            if repo_type == 'hg':
                hg = self.query_exe('hg', return_type='list')
                revision = self.get_output_from_command(
                    hg + ['parent', '--template', '{node}'], cwd=repo
                )
            elif repo_type == 'git':
                git = self.query_exe('git', return_type='list')
                revision = self.get_output_from_command(
                    git + ['rev-parse', 'HEAD'], cwd=repo
                )
            else:
                return None
        return revision

    def query_gecko_config_path(self):
        conf_file = self.config.get('gecko_config')
        if conf_file is None:
            conf_file = os.path.join(
                'b2g', 'config',
                self.config.get('b2g_config_dir', self.config['target']),
                'config.json'
            )
        return conf_file

    def query_remote_gecko_config(self):
        repo = self.query_repo()
        if os.path.exists(repo):
            config_path = self.query_gecko_config_path()
            config_path = "{repo}/{config_path}".format(repo=repo, config_path=config_path)
            return json.load(open(config_path, "r"))
        elif detect_hg(repo):
            rev = self.query_revision()
            if rev is None:
                rev = 'default'

            config_path = self.query_gecko_config_path()
            # Handle local files vs. in-repo files
            url = self.query_hgweb_url(repo, rev, config_path)
            return self.retry(self.load_json_from_url, args=(url,))
        elif detect_git(repo):
            rev = self.query_revision()
            if rev is None:
                rev = 'HEAD'

            config_path = self.query_gecko_config_path()
            url = self.query_gitweb_url(repo, rev, config_path)
            return self.retry(self.load_json_from_url, args=(url,))

    def load_gecko_config(self):
        if self.gecko_config:
            return self.gecko_config

        gecko_config = self._load_gecko_config()

        # Set up mock immediately so any later run_command_m doesn't end up
        # calling setup_mock with the wrong config.
        self.setup_mock(gecko_config['mock_target'],
                        gecko_config['mock_packages'],
                        gecko_config.get('mock_files'))

        return gecko_config

    def _load_gecko_config(self):
        if 'gecko_config' not in self.config:
            # Grab from the remote if we're not overriding on the cmdline
            self.gecko_config = self.query_remote_gecko_config()
            return self.gecko_config

        dirs = self.query_abs_dirs()
        conf_file = self.query_gecko_config_path()
        if not os.path.isabs(conf_file):
            conf_file = os.path.abspath(os.path.join(dirs['gecko_src'], conf_file))

        if os.path.exists(conf_file):
            self.info("gecko_config file: %s" % conf_file)
            self.run_command(['cat', conf_file])
            self.gecko_config = json.load(open(conf_file))
            return self.gecko_config

        # The file doesn't exist; let's try loading it remotely
        self.gecko_config = self.query_remote_gecko_config()
        return self.gecko_config

    def query_build_env(self):
        """Retrieves the environment for building"""
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        env = self.query_env()
        for k, v in gecko_config.get('env', {}).items():
            v = v.format(workdir=dirs['abs_work_dir'],
                         srcdir=os.path.abspath(dirs['gecko_src']))
            env[k] = v
        if self.config.get('env', {}).get('B2G_UPDATE_CHANNEL'):
            v = str(self.config['env']['B2G_UPDATE_CHANNEL'])
            env['B2G_UPDATE_CHANNEL'] = v
        if self.config.get('variant'):
            v = str(self.config['variant'])
            env['VARIANT'] = v
        if self.config.get('ccache'):
            env['CCACHE_BASEDIR'] = dirs['work_dir']
        # If we get a buildid from buildbot, pass that in as MOZ_BUILD_DATE
        if self.buildbot_config and 'buildid' in self.buildbot_config.get('properties', {}):
            env['MOZ_BUILD_DATE'] = self.buildbot_config['properties']['buildid']

        return env

    def query_hgweb_url(self, repo, rev, filename=None):
        if filename:
            url = "{baseurl}/raw-file/{rev}/{filename}".format(
                baseurl=repo,
                rev=rev,
                filename=filename)
        else:
            url = "{baseurl}/rev/{rev}".format(
                baseurl=repo,
                rev=rev)
        return url

    def query_gitweb_url(self, repo, rev, filename=None):
        # Git does not support raw files download, so each git
        # provider has its own way to make that possible
        if 'github.com' in repo or 'bitbucket.org' in repo:
            if filename:
                url = '{repo}/raw/{rev}/{filename}'.format(
                        repo=repo,
                        rev=rev,
                        filename=filename)
            else:
                url = '{repo}/raw/{rev}'.format(
                        repo=repo,
                        rev=rev)
        else:
            bits = urlparse.urlparse(repo)
            repo = bits.path.lstrip('/')
            if filename:
                url = "{scheme}://{host}/?p={repo};a=blob_plain;f={filename};hb={rev}".format(
                    scheme=bits.scheme,
                    host=bits.netloc,
                    repo=repo,
                    filename=filename,
                    rev=rev)
            else:
                url = "{scheme}://{host}/?p={repo};a=tree;h={rev}".format(
                    scheme=bits.scheme,
                    host=bits.netloc,
                    repo=repo,
                    rev=rev)
        return url


    # Actions {{{2
    def checkout_repotool(self, repo_dir):
        self.info("Checking out repo tool")
        repo_repo = self.config['repo_repo']
        repo_rev = self.config['repo_rev']
        repos = [
            {'vcs': 'gittool', 'repo': repo_repo, 'dest': repo_dir, 'revision': repo_rev},
        ]

        # self.vcs_checkout already retries, so no need to wrap it in
        # self.retry. We set the error_level to ERROR to prevent it going fatal
        # so we can do our own handling here.
        retval = self.vcs_checkout_repos(repos, error_level=ERROR)
        if not retval:
            self.rmtree(repo_dir)
            self.fatal("Automation Error: couldn't clone repo", exit_code=4)
        return retval

    def checkout_tools(self):
        dirs = self.query_abs_dirs()

        # We need hg.m.o/build/tools checked out
        self.info("Checking out tools")
        repos = [{
            'repo': self.config['tools_repo'],
            'vcs': "hg",  # May not have hgtool yet
            'dest': dirs['abs_tools_dir'],
        }]
        rev = self.vcs_checkout(**repos[0])
        self.set_buildbot_property("tools_revision", rev, write_to_file=True)

    def checkout_sources(self):
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        b2g_manifest_intree = gecko_config.get('b2g_manifest_intree')

        if gecko_config.get('config_version') >= 2:
            repos = [
                {'vcs': 'gittool', 'repo': 'https://git.mozilla.org/b2g/B2G.git', 'dest': dirs['work_dir']},
            ]

            if b2g_manifest_intree:
                # Checkout top-level B2G repo now
                self.vcs_checkout_repos(repos)
                b2g_manifest_branch = 'master'

                # That may have blown away our build-tools checkout. It would
                # be better if B2G were checked out into a subdirectory, but
                # for now, just redo it.
                self.checkout_tools()

                # Now checkout gecko inside the build directory
                self.checkout_gecko()
                conf_dir = os.path.join(dirs['gecko_src'], os.path.dirname(self.query_gecko_config_path()))
                manifest_filename = os.path.join(conf_dir, 'sources.xml')
                self.info("Using manifest at %s" % manifest_filename)
                have_gecko = True
            else:
                # Checkout B2G and b2g-manifests. We'll do gecko later
                b2g_manifest_branch = gecko_config.get('b2g_manifest_branch', 'master')
                repos.append(
                    {'vcs': 'gittool',
                     'repo': 'https://git.mozilla.org/b2g/b2g-manifest.git',
                     'dest': os.path.join(dirs['work_dir'], 'b2g-manifest'),
                     'branch': b2g_manifest_branch},
                )
                manifest_filename = gecko_config.get('b2g_manifest', self.config['target'] + '.xml')
                manifest_filename = os.path.join(dirs['work_dir'], 'b2g-manifest', manifest_filename)
                self.vcs_checkout_repos(repos)
                have_gecko = False

            manifest = load_manifest(manifest_filename)

            if not b2g_manifest_intree:
                # Now munge the manifest by mapping remotes to local remotes
                mapping_func = functools.partial(map_remote, mappings=self.config['repo_remote_mappings'])

                rewrite_remotes(manifest, mapping_func)
                # Remove gecko, since we'll be checking that out ourselves
                gecko_node = remove_project(manifest, path='gecko')
                if not gecko_node:
                    self.fatal("couldn't remove gecko from manifest")

            # Write out our manifest locally
            manifest_dir = os.path.join(dirs['work_dir'], 'tmp_manifest')
            self.rmtree(manifest_dir)
            self.mkdir_p(manifest_dir)
            manifest_filename = os.path.join(manifest_dir, self.config['target'] + '.xml')
            self.info("Writing manifest to %s" % manifest_filename)
            manifest_file = open(manifest_filename, 'w')
            manifest.writexml(manifest_file)
            manifest_file.close()

            # Set up repo
            repo_link = os.path.join(dirs['work_dir'], '.repo')
            if 'repo_mirror_dir' in self.config:
                # Make our local .repo directory a symlink to the shared repo
                # directory
                repo_mirror_dir = self.config['repo_mirror_dir']
                self.mkdir_p(repo_mirror_dir)
                repo_link = os.path.join(dirs['work_dir'], '.repo')
                if not os.path.exists(repo_link) or not os.path.islink(repo_link):
                    self.rmtree(repo_link)
                    self.info("Creating link from %s to %s" % (repo_link, repo_mirror_dir))
                    os.symlink(repo_mirror_dir, repo_link)

            # Checkout the repo tool
            if 'repo_repo' in self.config:
                repo_dir = os.path.join(dirs['work_dir'], '.repo', 'repo')
                self.checkout_repotool(repo_dir)

                cmd = ['./repo', '--version']
                if not self.run_command(cmd, cwd=dirs['work_dir']) == 0:
                    # Set return code to RETRY
                    self.fatal("repo is broken", exit_code=4)

            # Check it out!
            max_tries = 5
            sleep_time = 60
            max_sleep_time = 300
            for _ in range(max_tries):
                # If .repo points somewhere, then try and reset our state
                # before running config.sh
                if os.path.isdir(repo_link):
                    # Delete any projects with broken HEAD references
                    self.info("Deleting broken projects...")
                    cmd = ['./repo', 'forall', '-c', 'git show-ref -q --head HEAD || rm -rfv $PWD']
                    self.run_command(cmd, cwd=dirs['work_dir'])

                # bug https://bugzil.la/1177190 - workaround - change
                # timeout from 55 to 10 min, based on "normal" being
                # about 7.5 minutes
                config_result = self.run_command([
                    './config.sh', '-q', self.config['target'], manifest_filename,
                ], cwd=dirs['work_dir'], output_timeout=10 * 60)

                # TODO: Check return code from these? retry?
                # Run git reset --hard to make sure we're in a clean state
                self.info("Resetting all git projects")
                cmd = ['./repo', 'forall', '-c', 'git reset --hard']
                self.run_command(cmd, cwd=dirs['work_dir'])

                self.info("Cleaning all git projects")
                cmd = ['./repo', 'forall', '-c', 'git clean -f -x -d']
                self.run_command(cmd, cwd=dirs['work_dir'])

                if config_result == 0:
                    break
                else:
                    # We may have died due to left-over lock files. Make sure
                    # we clean those up before trying again.
                    self.info("Deleting stale lock files")
                    cmd = ['find', '.repo/', '-name', '*.lock', '-print', '-delete']
                    self.run_command(cmd, cwd=dirs['work_dir'])

                    # Try again in a bit. Broken clones should be deleted and
                    # re-tried above
                    self.info("config.sh failed; sleeping %i and retrying" % sleep_time)
                    time.sleep(sleep_time)
                    # Exponential backoff with random jitter
                    sleep_time = min(sleep_time * 1.5, max_sleep_time) + random.randint(1, 60)
            else:
                self.fatal("failed to run config.sh")

            # Workaround bug 985837
            if self.config['target'] == 'emulator-kk':
                self.info("Forcing -j4 for emulator-kk")
                dotconfig_file = os.path.join(dirs['abs_work_dir'], '.config')
                with open(dotconfig_file, "a+") as f:
                    f.write("\nMAKE_FLAGS=-j1\n")

            # output our sources.xml, make a copy for update_sources_xml()
            self.run_command(
                ["./gonk-misc/add-revision.py", "-o", "sources.xml", "--force",
                 ".repo/manifest.xml"], cwd=dirs["work_dir"],
                halt_on_failure=True, fatal_exit_code=3)
            self.run_command(["cat", "sources.xml"], cwd=dirs['work_dir'], halt_on_failure=True, fatal_exit_code=3)
            self.run_command(["cp", "-p", "sources.xml", "sources.xml.original"], cwd=dirs['work_dir'], halt_on_failure=True, fatal_exit_code=3)

            manifest = load_manifest(os.path.join(dirs['work_dir'], 'sources.xml'))
            gaia_node = get_project(manifest, path="gaia")
            gaia_rev = gaia_node.getAttribute("revision")
            gaia_remote = get_remote(manifest, gaia_node.getAttribute('remote'))
            gaia_repo = "%s/%s" % (gaia_remote.getAttribute('fetch'), gaia_node.getAttribute('name'))
            gaia_url = self.query_gitweb_url(gaia_repo, gaia_rev)
            self.set_buildbot_property("gaia_revision", gaia_rev, write_to_file=True)
            self.info("TinderboxPrint: gaia_revlink: %s" % gaia_url)

            # Now we can checkout gecko and other stuff
            if not have_gecko:
                self.checkout_gecko()
            return

        # Old behaviour
        self.checkout_gecko()
        self.checkout_gaia()

    def checkout_gecko(self):
        '''
        If you want a different revision of gecko to be used you can use the
        --checkout-revision flag. This is necessary for trees that are not
        triggered by a gecko commit but by an external tree (like gaia).
        '''
        dirs = self.query_abs_dirs()

        # Make sure the parent directory to gecko exists so that 'hg share ...
        # build/gecko' works
        self.mkdir_p(os.path.dirname(dirs['gecko_src']))

        repo = self.query_repo()
        if "checkout_revision" in self.config:
            rev = self.vcs_checkout(repo=repo, dest=dirs['gecko_src'], revision=self.config["checkout_revision"])
            # in this case, self.query_revision() will be returning the "revision" that triggered the job
            # we know that it is not a gecko revision that did so
            self.set_buildbot_property('revision', self.query_revision(), write_to_file=True)
        else:
            # a gecko revision triggered this job; self.query_revision() will return it
            rev = self.vcs_checkout(repo=repo, dest=dirs['gecko_src'], revision=self.query_revision())
            self.set_buildbot_property('revision', rev, write_to_file=True)
        self.set_buildbot_property('gecko_revision', rev, write_to_file=True)

    def checkout_gaia(self):
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        gaia_config = gecko_config.get('gaia')
        if gaia_config:
            dest = os.path.join(dirs['abs_work_dir'], 'gaia')
            repo = gaia_config['repo']
            branch = gaia_config.get('branch')
            vcs = gaia_config['vcs']
            rev = self.vcs_checkout(repo=repo, dest=dest, vcs=vcs, branch=branch)
            self.set_buildbot_property('gaia_revision', rev, write_to_file=True)
            self.info("TinderboxPrint: gaia_revlink: %s/rev/%s" % (repo, rev))
