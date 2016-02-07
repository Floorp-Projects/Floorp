"""
Module for performing gaia-specific tasks
"""

import json
import os
import re

from mozharness.base.errors import HgErrorList, BaseErrorList, TarErrorList, \
    ZipErrorList
from mozharness.base.log import ERROR, FATAL

gaia_config_options = [
    [["--gaia-dir"],
    {"action": "store",
     "dest": "gaia_dir",
     "help": "directory where gaia repo should be cloned"
     }],
    [["--gaia-repo"],
    {"action": "store",
     "dest": "gaia_repo",
     "help": "url of gaia repo to clone"
    }],
    [["--gaia-branch"],
    {"action": "store",
     "dest": "gaia_branch",
     "default": "default",
     "help": "branch of gaia repo to clone"
    }],
]

class GaiaMixin(object):

    npm_error_list = BaseErrorList + [
        {'substr': r'''npm ERR! Error:''', 'level': ERROR}
    ]

    # This requires self to inherit a VCSMixin.
    def clone_gaia(self, dest, repo, use_gaia_json=False):
        """
        Clones an hg mirror of gaia.

        repo: a dict containing 'repo_path', 'revision', and optionally
              'branch' parameters
        use_gaia_json: if True, the repo parameter is used to retrieve
              a gaia.json file from a gecko repo, which in turn is used to
              clone gaia; if False, repo represents a gaia repo to clone.
        """

        repo_path = repo.get('repo_path')
        revision = repo.get('revision')
        branch = repo.get('branch')
        gaia_json_path = self.config.get("gaia_json_path", "{repo_path}/raw-file/{revision}/b2g/config/gaia.json")
        git = False
        pr_git_revision = None
        pr_remote = None

        self.info('dest: %s' % dest)

        if use_gaia_json:
            url = gaia_json_path.format(
                repo_path=repo_path,
                revision=revision)
            contents = self.retry(self.load_json_from_url, args=(url,))
            if contents.get('git') and contents['git'].get('remote'):
                git = True
                remote = contents['git']['remote']
                branch = contents['git'].get('branch')
                revision = contents['git'].get('git_revision')
                pr_git_revision = contents['git'].get('pr_git_revision')
                pr_remote = contents['git'].get('pr_remote')
                if pr_remote or pr_git_revision:
                    if not (pr_remote and pr_git_revision):
                        self.fatal('Pull request mode requres rev *and* remote')
                if not (branch or revision):
                    self.fatal('Must specify branch or revision for git repo')
            elif contents.get('repo_path') and contents.get('revision'):
                repo_path = 'https://hg.mozilla.org/%s' % contents['repo_path']
                revision = contents['revision']
                branch = None

        if git:
            git_cmd = self.query_exe('git')
            needs_clobber = True

            # For pull requests, we only want to clobber when we can't find the
            # two exact commit ids that we'll be working with.  As long as we
            # have those two commits, we don't care about the rest of the repo
            def has_needed_commit(commit, fatal=False):
                cmd = [git_cmd, 'rev-parse', '--quiet', '--verify', '%s^{commit}' % commit]
                rc = self.run_command(cmd, cwd=dest, halt_on_failure=False, success_codes=[1,0])
                if rc != 0:
                    return False
                return True

            if not pr_remote and os.path.exists(dest) and os.path.exists(os.path.join(dest, '.git')):
                cmd = [git_cmd, 'remote', '-v']
                output = self.get_output_from_command(cmd, cwd=dest)
                for line in output:
                    if remote in line:
                        needs_clobber = False


            # We want to do some cleanup logic differently for pull requests
            if pr_git_revision and pr_remote:
                needs_clobber = False
                if os.path.exists(dest) and os.path.exists(os.path.join(dest, '.git')):
                    cmd = [git_cmd, 'clean', '-f', '-f', '-x', '-d']
                    self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                     fatal_exit_code=3)
                    if not has_needed_commit(revision):
                        cmd = [git_cmd, 'fetch', 'origin']
                        self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                         fatal_exit_code=3)
                    if not has_needed_commit(revision):
                        self.warn('Repository does not contain required revisions, clobbering')
                        needs_clobber = True

            # In pull request mode, we don't want to clone because we're satisfied
            # that the base directory is good enough because
            needs_clone = True
            if pr_git_revision and pr_remote:
                if os.path.exists(dest):
                    if os.path.exists(os.path.join(dest, '.git')):
                        needs_clone = False
                    else:
                        needs_clobber = True

            if needs_clobber:
                self.rmtree(dest)
                needs_clone = True

            if needs_clone:
                # git clone
                cmd = [git_cmd,
                       'clone',
                       remote]
                self.run_command(cmd,
                                 cwd=os.path.dirname(dest),
                                 output_timeout=1760,
                                 halt_on_failure=True,
                                 fatal_exit_code=3)

            self.run_command([git_cmd, 'status'], cwd=dest);


            # handle pull request magic
            if pr_git_revision and pr_remote:
                # Optimization opportunity: instead of fetching all remote references,
                # pull only the single commit.  I don't know how to right now

                # If the 'other' remote exists, get rid of it
                cmd = [git_cmd, 'remote']
                output = self.get_output_from_command(cmd, cwd=dest)
                for line in output.split('\n'):
                  if 'other' in line:
                    cmd = [git_cmd, 'remote', 'rm', 'other']
                    self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                     fatal_exit_code=3)
                    break;
                # Set the correct remote
                cmd = [git_cmd, 'remote', 'add', 'other', pr_remote]
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)
                if not has_needed_commit(pr_git_revision):
                    cmd = [git_cmd, 'fetch', 'other']
                    self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                     fatal_exit_code=3)
                if not has_needed_commit(pr_git_revision):
                    self.fatal('Missing the Pull Request target revision')

                # With these environment variables we should have deterministic
                # merge commit identifiers
                self.info('If you want to prove that this merge commit is the same')
                self.info('you get, use this environment while doing the merge')
                env = {
                  'GIT_COMMITTER_DATE': "Wed Feb 16 14:00 2037 +0100",
                  'GIT_AUTHOR_DATE': "Wed Feb 16 14:00 2037 +0100",
                  'GIT_AUTHOR_NAME': 'automation',
                  'GIT_AUTHOR_EMAIL': 'auto@mati.on',
                  'GIT_COMMITTER_NAME': 'automation',
                  'GIT_COMMITTER_EMAIL': 'auto@mati.on'
                }
                cmd = [git_cmd, 'reset', '--hard', revision or branch]
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)
                cmd = [git_cmd, 'clean', '-f', '-f', '-x', '-d']
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)
                cmd = [git_cmd, 'merge', '--no-ff', pr_git_revision]
                self.run_command(cmd, cwd=dest, env=env, halt_on_failure=True,
                                 fatal_exit_code=3)
                # So that people can verify that their merge commit is identical
                cmd = [git_cmd, 'rev-parse', 'HEAD']
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)

            else:
                # checkout git branch
                cmd = [git_cmd,
                       'checkout',
                       revision or branch]
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)


            # verify
            for cmd in ([git_cmd, 'rev-parse', 'HEAD'], [git_cmd, 'branch']):
                self.run_command(cmd, cwd=dest, halt_on_failure=True,
                                 fatal_exit_code=3)

        else:
            # purge the repo if it already exists
            if os.path.exists(dest):
                if os.path.exists(os.path.join(dest, '.hg')):
                    # this is an hg dir, so do an hg clone
                    cmd = [self.query_exe('hg'),
                           '--config',
                           'extensions.purge=',
                           'purge']
                    if self.run_command(cmd, cwd=dest, error_list=HgErrorList):
                        self.fatal("Unable to purge %s!" % dest)
                else:
                    # there's something here, but it isn't hg; just delete it
                    self.rmtree(dest)

            repo = {
                'repo': repo_path,
                'revision': revision,
                'branch': branch,
                'dest': dest,
            }

            self.vcs_checkout_repos([repo], parent_dir=os.path.dirname(dest))

    def preflight_pull(self):
        if not self.buildbot_config:
            if not self.config.get('gaia_repo'):
                # gaia_branch by default is set to default
                self.fatal("You're trying to run this outside of buildbot, " \
                    "therefore, you need to specify --gaia-repo.")
            if not self.config.get('gaia_branch'):
                # gaia_branch by default is set to default
                self.fatal("You're trying to run this outside of buildbot, " \
                    "therefore, you need to specify --gaia-branch.")

    def extract_xre(self, xre_url, xre_path=None, parent_dir=None):
        if not xre_path:
            xre_path = self.config.get('xre_path')
        xulrunner_bin = os.path.join(parent_dir,
                                     xre_path,
                                     'bin', 'xpcshell')
        if os.access(xulrunner_bin, os.F_OK):
            return

        filename = self.download_file(xre_url, parent_dir=parent_dir)
        m = re.search('\.tar\.(bz2|gz)$', filename)
        if m:
            # a xulrunner archive, which has a top-level 'xulrunner-sdk' dir
            command = self.query_exe('tar', return_type='list')
            tar_cmd = "jxf"
            if m.group(1) == "gz":
                tar_cmd = "zxf"
            command.extend([tar_cmd, filename])
            self.run_command(command,
                             cwd=parent_dir,
                             error_list=TarErrorList,
                             halt_on_failure=True,
                             fatal_exit_code=3)
        else:
            # a tooltool xre.zip
            command = self.query_exe('unzip', return_type='list')
            command.extend(['-q', '-o', filename])
            # Gaia assumes that xpcshell is in a 'xulrunner-sdk' dir, but
            # xre.zip doesn't have a top-level directory name, so we'll
            # create it.
            parent_dir = os.path.join(parent_dir,
                                      self.config.get('xre_path'))
            if not os.access(parent_dir, os.F_OK):
                self.mkdir_p(parent_dir, error_level=FATAL)
            self.run_command(command,
                             cwd=parent_dir,
                             error_list=ZipErrorList,
                             halt_on_failure=True,
                             fatal_exit_code=3)

    def pull(self, **kwargs):
        '''
        Two ways of using this function:
        - The user specifies --gaia-repo or in a config file
        - The buildbot propeties exist and we query the gaia json url
          for the current gecko tree
        '''
        dirs = self.query_abs_dirs()
        dest = dirs['abs_gaia_dir']
        repo = {}

        if self.buildbot_config is not None:
            # get gaia commit via hgweb (gecko's gaia.json)
            repo = {
                'revision': self.buildbot_config['properties']['revision'],
                'repo_path': 'https://hg.mozilla.org/%s' % self.buildbot_config['properties']['repo_path'],
                'branch': None,
            }
        else:
            repo = {
                'repo_path': self.config['gaia_repo'],
                'revision': 'default',
                'branch': self.config['gaia_branch']
            }

        self.clone_gaia(dest, repo,
                        use_gaia_json=self.buildbot_config is not None)

    def make_gaia(self, gaia_dir, xre_dir, debug=False, noftu=True,
                  xre_url=None, build_config_path=None):
        if xre_url:
            self.extract_xre(xre_url, xre_path=xre_dir, parent_dir=gaia_dir)

        env = {'DEBUG': '1' if debug else '0',
               'NOFTU': '1' if noftu else '0',
               'DESKTOP': '0',
               'DESKTOP_SHIMS': '1',
               'USE_LOCAL_XULRUNNER_SDK': '1',
               'XULRUNNER_DIRECTORY': xre_dir
              }

        # if tbpl_build_config.json exists, load it
        if build_config_path:
            if os.path.exists(build_config_path):
                with self.opened(build_config_path) as (f, err):
                    if err:
                        self.fatal("Error while reading %s, aborting" %
                                   build_config_path)
                    else:
                        contents = f.read()
                        config = json.loads(contents)
                        env.update(config.get('env', {}))

        self.info('Sending environment as make vars because of bug 1028816')

        cmd = self.query_exe('make', return_type="list")
        for key, value in env.iteritems():
            cmd.append('%s=%s' % (key, value))
        self.run_command(cmd,
                         cwd=gaia_dir,
                         halt_on_failure=True)

    def make_node_modules(self):

        dirs = self.query_abs_dirs()

        def cleanup_node_modules():
            node_dir = os.path.join(dirs['abs_gaia_dir'], 'node_modules')
            self.rmtree(node_dir)

        self.run_command(['npm', 'cache', 'clean'])

        # get the node modules first from the node module cache, if this fails it will 
        # install the node modules from npmjs.org.
        cmd = ['taskcluster-npm-cache-get',
               '--namespace',
               'gaia.npm_cache',
               'package.json']
        kwargs = {
            'output_timeout': 300
        }
        code = self.retry(self.run_command, attempts=3, good_statuses=(0,),
                          args=[cmd, dirs['abs_gaia_dir']], cleanup=cleanup_node_modules, kwargs=kwargs)
        if code:
            self.fatal('Error occurred fetching node modules from cache',
                       exit_code=code)

        # run 'make node_modules' second, so we can separately handle
        # errors that occur here
        cmd = ['make',
               'node_modules',
               'NODE_MODULES_SRC=npm-cache',
               'VIRTUALENV_EXISTS=1']
        kwargs = {
            'output_timeout': 300,
            'error_list': self.npm_error_list
        }
        code = self.retry(self.run_command, attempts=3, good_statuses=(0,),
                          args=[cmd, dirs['abs_gaia_dir']], cleanup=cleanup_node_modules, kwargs=kwargs)
        if code:
            # Dump npm-debug.log, if it exists
            npm_debug = os.path.join(dirs['abs_gaia_dir'], 'npm-debug.log')
            if os.access(npm_debug, os.F_OK):
                self.info('dumping npm-debug.log')
                self.run_command(['cat', npm_debug])
            else:
                self.info('npm-debug.log doesn\'t exist, not dumping')
            self.fatal('Errors during \'npm install\'', exit_code=code)

        cmd = ['make',
               'update-common']
        kwargs = {
            'output_timeout': 300
        }
        code = self.retry(self.run_command, attempts=3, good_statuses=(0,),
                          args=[cmd, dirs['abs_gaia_dir']], kwargs=kwargs)
        if code:
            self.fatal('Errors during make update-common')

    def node_setup(self):
        """
        Set up environment for node-based Gaia tests.
        """
        dirs = self.query_abs_dirs()

        # Set-up node modules for project first. They must be older than 
        # the b2g build we copy into place. Otherwise the b2g build will
        # be considered an out of date target and we don't want that! It
        # can cause our desired b2g-desktop build to be overwritten!
        self.make_node_modules()

        # Copy the b2g desktop we built to the gaia directory so that it
        # gets used by the marionette-js-runner.
        b2g_dest = os.path.join(dirs['abs_gaia_dir'], 'b2g')
        self.copytree(
            os.path.join(os.path.dirname(self.binary_path)),
            b2g_dest,
            overwrite='clobber'
        )
        # Ensure modified time is more recent than node_modules!
        self.run_command(['touch', '-c', b2g_dest])

