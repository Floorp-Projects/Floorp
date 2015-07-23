#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
from datetime import datetime
from functools import wraps

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.errors import MakefileErrorList
from mozharness.base.script import BaseScript
from mozharness.base.transfer import TransferMixin
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.building.hazards import HazardError, HazardAnalysis
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.mock import MockMixin
from mozharness.mozilla.tooltool import TooltoolMixin

SUCCESS, WARNINGS, FAILURE, EXCEPTION, RETRY = xrange(5)


def requires(*queries):
    """Wrapper for detecting problems where some bit of information
    required by the wrapped step is unavailable. Use it put prepending
    @requires("foo"), which will check whether self.query_foo() returns
    something useful."""
    def make_wrapper(f):
        @wraps(f)
        def wrapper(self, *args, **kwargs):
            for query in queries:
                val = query(self)
                goodval = not (val is None or "None" in str(val))
                assert goodval, f.__name__ + " requires " + query.__name__ + " to return a value"
            return f(self, *args, **kwargs)
        return wrapper
    return make_wrapper


nuisance_env_vars = ['TERMCAP', 'LS_COLORS', 'PWD', '_']


class SpidermonkeyBuild(MockMixin,
                        PurgeMixin, BaseScript,
                        VCSMixin, BuildbotMixin, TooltoolMixin, TransferMixin):
    config_options = [
        [["--repo"], {
            "dest": "repo",
            "help": "which gecko repo to get spidermonkey from",
        }],
        [["--source"], {
            "dest": "source",
            "help": "directory containing gecko source tree (instead of --repo)",
        }],
        [["--revision"], {
            "dest": "revision",
        }],
        [["--branch"], {
            "dest": "branch",
        }],
        [["--vcs-share-base"], {
            "dest": "vcs_share_base",
            "help": "base directory for shared repositories",
        }],
        [["-j"], {
            "dest": "concurrency",
            "type": int,
            "default": 4,
            "help": "number of simultaneous jobs used while building the shell " +
                    "(currently ignored for the analyzed build",
        }],
    ]

    def __init__(self):
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            # other stuff
                            all_actions=[
                                'purge',
                                'checkout-tools',

                                # First, build an optimized JS shell for running the analysis
                                'checkout-source',
                                'get-blobs',
                                'clobber-shell',
                                'configure-shell',
                                'build-shell',

                                # Next, build a tree with the analysis plugin
                                # active. Note that we are using the same
                                # checkout for the JS shell build and the build
                                # of the source to be analyzed, which is a
                                # little unnecessary (no need to rebuild the JS
                                # shell all the time). (Different objdir,
                                # though.)
                                'clobber-analysis',
                                'setup-analysis',
                                'run-analysis',
                                'collect-analysis-output',
                                'upload-analysis',
                                'check-expectations',
                            ],
                            default_actions=[
                                'purge',
                                'checkout-tools',
                                'checkout-source',
                                'get-blobs',
                                'clobber-shell',
                                'configure-shell',
                                'build-shell',
                                'clobber-analysis',
                                'setup-analysis',
                                'run-analysis',
                                'collect-analysis-output',
                                'upload-analysis',
                                'check-expectations',
                            ],
                            config={
                                'default_vcs': 'hgtool',
                                'vcs_share_base': os.environ.get('HG_SHARE_BASE_DIR'),
                                'ccache': True,
                                'buildbot_json_path': os.environ.get('PROPERTIES_FILE'),
                                'tools_repo': 'https://hg.mozilla.org/build/tools',

                                'upload_ssh_server': None,
                                'upload_remote_basepath': None,
                                'enable_try_uploads': True,
                                'source': None,
                            },
        )

        self.buildid = None
        self.analysis = HazardAnalysis()

    def _pre_config_lock(self, rw_config):
        if self.config['source']:
            self.config['srcdir'] = self.config['source']
        super(SpidermonkeyBuild, self)._pre_config_lock(rw_config)

        if self.buildbot_config is None:
            self.info("Reading buildbot build properties...")
            self.read_buildbot_config()

        if self.buildbot_config:
            bb_props = [('mock_target', 'mock_target', None),
                        ('base_bundle_urls', 'hgtool_base_bundle_urls', None),
                        ('base_mirror_urls', 'hgtool_base_mirror_urls', None),
                        ('hgurl', 'hgurl', None),
                        ('clobberer_url', 'clobberer_url', 'https://api.pub.build.mozilla.org/clobberer/lastclobber'),
                        ('purge_minsize', 'purge_minsize', 15),
                        ('purge_maxage', 'purge_maxage', None),
                        ('purge_skip', 'purge_skip', None),
                        ('force_clobber', 'force_clobber', None),
                        ]
            buildbot_props = self.buildbot_config.get('properties', {})
            for bb_prop, cfg_prop, default in bb_props:
                if not self.config.get(cfg_prop) and buildbot_props.get(bb_prop, default):
                    self.config[cfg_prop] = buildbot_props.get(bb_prop, default)
            self.config['is_automation'] = True
        else:
            self.config['is_automation'] = False

        dirs = self.query_abs_dirs()
        replacements = self.config['env_replacements'].copy()
        for k,v in replacements.items():
            replacements[k] = v % dirs

        self.env = self.query_env(replace_dict=replacements,
                                  partial_env=self.config['partial_env'],
                                  purge_env=nuisance_env_vars)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = BaseScript.query_abs_dirs(self)

        abs_work_dir = abs_dirs['abs_work_dir']
        dirs = {
            'shell_objdir':
                os.path.join(abs_work_dir, self.config['shell-objdir']),
            'mozharness_scriptdir':
                os.path.abspath(os.path.dirname(__file__)),
            'abs_analysis_dir':
                os.path.join(abs_work_dir, self.config['analysis-dir']),
            'abs_analyzed_objdir':
                os.path.join(abs_work_dir, self.config['srcdir'], self.config['analysis-objdir']),
            'analysis_scriptdir':
                os.path.join(self.config['srcdir'], self.config['analysis-scriptdir']),
            'abs_tools_dir':
                os.path.join(abs_dirs['base_work_dir'], 'tools'),
            'gecko_src':
                os.path.join(abs_work_dir, self.config['srcdir']),
            'abs_blob_upload_dir':
                os.path.join(abs_work_dir, 'blobber_upload_dir'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def query_repo(self):
        if self.config.get('repo'):
            return self.config['repo']
        elif self.buildbot_config and 'properties' in self.buildbot_config:
            return self.config['hgurl'] + self.buildbot_config['properties']['repo_path']
        else:
            return None

    def query_revision(self):
        if 'revision' in self.buildbot_properties:
            revision = self.buildbot_properties['revision']
        elif self.buildbot_config and 'sourcestamp' in self.buildbot_config:
            revision = self.buildbot_config['sourcestamp']['revision']
        else:
            # Useful for local testing. In actual use, this would always be
            # None.
            revision = self.config.get('revision')

        return revision[0:12] if revision else None

    def query_branch(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['branch']
        elif 'branch' in self.config:
            # Used for locally testing try vs non-try
            return self.config['branch']
        else:
            return os.path.basename(self.query_repo())

    def query_compiler_manifest(self):
        dirs = self.query_abs_dirs()
        manifest = os.path.join(dirs['abs_work_dir'], dirs['analysis_scriptdir'], self.config['compiler_manifest'])
        if os.path.exists(manifest):
            return manifest
        return os.path.join(dirs['abs_work_dir'], self.config['compiler_manifest'])

    def query_sixgill_manifest(self):
        dirs = self.query_abs_dirs()
        manifest = os.path.join(dirs['abs_work_dir'], dirs['analysis_scriptdir'], self.config['sixgill_manifest'])
        if os.path.exists(manifest):
            return manifest
        return os.path.join(dirs['abs_work_dir'], self.config['sixgill_manifest'])

    def query_buildid(self):
        if self.buildid:
            return self.buildid
        if self.buildbot_config and 'properties' in self.buildbot_config:
            self.buildid = self.buildbot_config['properties'].get('buildid')
        if not self.buildid:
            self.buildid = datetime.now().strftime("%Y%m%d%H%M%S")
        return self.buildid

    def query_upload_ssh_server(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['upload_ssh_server']
        else:
            return self.config['upload_ssh_server']

    def query_upload_ssh_key(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            key = self.buildbot_config['properties']['upload_ssh_key']
        else:
            key = self.config['upload_ssh_key']
        if self.mock_enabled and not key.startswith("/"):
            key = "/home/mock_mozilla/.ssh/" + key
        return key

    def query_upload_ssh_user(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['upload_ssh_user']
        else:
            return self.config['upload_ssh_user']

    def query_product(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['product']
        else:
            return self.config['product']

    def query_upload_remote_basepath(self):
        if self.config.get('upload_remote_basepath'):
            return self.config['upload_remote_basepath']
        else:
            return "/pub/mozilla.org/{product}".format(
                product=self.query_product(),
            )

    def query_upload_remote_baseuri(self):
        baseuri = self.config.get('upload_remote_baseuri')
        if self.buildbot_config and 'properties' in self.buildbot_config:
            buildprops = self.buildbot_config['properties']
            if 'upload_remote_baseuri' in buildprops:
                baseuri = buildprops['upload_remote_baseuri']
        return baseuri.strip("/") if baseuri else None

    def query_target(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['platform']
        else:
            return self.config.get('target')

    def query_upload_path(self):
        branch = self.query_branch()

        common = {
            'basepath': self.query_upload_remote_basepath(),
            'branch': branch,
            'target': self.query_target(),
        }

        if branch == 'try':
            if not self.config['enable_try_uploads']:
                return None
            try:
                user = self.buildbot_config['sourcestamp']['changes'][0]['who']
            except (KeyError, TypeError):
                user = "unknown"
            return "{basepath}/try-builds/{user}-{rev}/{branch}-{target}".format(
                user=user,
                rev=self.query_revision(),
                **common
            )
        else:
            return "{basepath}/tinderbox-builds/{branch}-{target}/{buildid}".format(
                buildid=self.query_buildid(),
                **common
            )

    def query_do_upload(self):
        if self.query_branch() == 'try':
            return self.config.get('enable_try_uploads')
        return True

    # Actions {{{2
    def purge(self):
        dirs = self.query_abs_dirs()
        self.info("purging, abs_upload_dir=" + dirs['abs_upload_dir'])
        PurgeMixin.clobber(
            self,
            always_clobber_dirs=[
                dirs['abs_upload_dir'],
            ],
        )

    def checkout_tools(self):
        dirs = self.query_abs_dirs()
        rev = self.vcs_checkout(
            vcs='hg',  # Don't have hgtool.py yet
            repo=self.config['tools_repo'],
            clean=False,
            dest=dirs['abs_tools_dir'],
        )
        self.set_buildbot_property("tools_revision", rev, write_to_file=True)

    def do_checkout_source(self):
        # --source option means to use an existing source directory instead of checking one out.
        if self.config['source']:
            return

        dirs = self.query_abs_dirs()
        dest = dirs['gecko_src']

        # Pre-create the directory to appease the share extension
        if not os.path.exists(dest):
            self.mkdir_p(dest)

        rev = self.vcs_checkout(
            repo=self.query_repo(),
            dest=dest,
            revision=self.query_revision(),
            branch=self.config.get('branch'),
            clean=True,
        )
        self.set_buildbot_property('source_revision', rev, write_to_file=True)

    def checkout_source(self):
        try:
            self.do_checkout_source()
        except Exception as e:
            self.fatal("checkout failed: " + str(e), exit_code=RETRY)

    def get_blobs(self):
        work_dir = self.query_abs_dirs()['abs_work_dir']
        if not os.path.exists(work_dir):
            self.mkdir_p(work_dir)
        self.tooltool_fetch(self.query_compiler_manifest(), output_dir=work_dir)
        self.tooltool_fetch(self.query_sixgill_manifest(), output_dir=work_dir)

    def clobber_shell(self):
        self.analysis.clobber_shell(self)

    def configure_shell(self):
        self.enable_mock()

        try:
            self.analysis.configure_shell(self)
        except HazardError as e:
            self.fatal(e, exit_code=FAILURE)

        self.disable_mock()

    def build_shell(self):
        self.enable_mock()

        try:
            self.analysis.build_shell(self)
        except HazardError as e:
            self.fatal(e, exit_code=FAILURE)

        self.disable_mock()

    def clobber_analysis(self):
        self.analysis.clobber(self)

    def setup_analysis(self):
        self.analysis.setup(self)

    def run_analysis(self):
        self.enable_mock()

        upload_dir = self.query_abs_dirs()['abs_blob_upload_dir']
        if not os.path.exists(upload_dir):
            self.mkdir_p(upload_dir)

        env = self.env.copy()
        env['MOZ_UPLOAD_DIR'] = upload_dir

        try:
            self.analysis.run(self, env=env, error_list=MakefileErrorList)
        except HazardError as e:
            self.fatal(e, exit_code=FAILURE)

        self.disable_mock()

    def collect_analysis_output(self):
        self.analysis.collect_output(self)

    def upload_analysis(self):
        if not self.config['is_automation']:
            return

        if not self.query_do_upload():
            self.info("Uploads disabled for this build. Skipping...")
            return

        self.enable_mock()

        try:
            self.analysis.upload_results(self)
        except HazardError as e:
            self.error(e)
            self.return_code = WARNINGS

        self.disable_mock()

    def check_expectations(self):
        try:
            self.analysis.check_expectations(self)
        except HazardError as e:
            self.fatal(e, exit_code=FAILURE)


# main {{{1
if __name__ == '__main__':
    myScript = SpidermonkeyBuild()
    myScript.run_and_exit()
