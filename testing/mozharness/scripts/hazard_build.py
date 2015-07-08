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
from mozharness.mozilla.building.buildb2gbase import B2GBuildBaseScript, B2GMakefileErrorList
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.building.hazards import HazardAnalysis, HazardError

SUCCESS, WARNINGS, FAILURE, EXCEPTION, RETRY = xrange(5)

nuisance_env_vars = ['TERMCAP', 'LS_COLORS', 'PWD', '_']


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
                assert (val is not None and "None" not in str(val)), "invalid " + query.__name__
            return f(self, *args, **kwargs)
        return wrapper
    return make_wrapper


class B2GHazardBuild(PurgeMixin, B2GBuildBaseScript):
    all_actions = [
        'clobber',
        'checkout-tools',
        'checkout-sources',
        'get-blobs',
        'update-source-manifest',
        'clobber-shell',
        'configure-shell',
        'build-shell',
        'clobber-analysis',
        'setup-analysis',
        'run-analysis',
        'collect-analysis-output',
        'upload-analysis',
        'check-expectations',
    ]

    default_actions = [
        'clobber',
        'checkout-tools',
        'checkout-sources',
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
    ]

    def __init__(self):
        super(B2GHazardBuild, self).__init__(
            config_options=[],
            config={
                'default_vcs': 'hgtool',
                'ccache': False,
                'mozilla_dir': 'build/gecko',

                'upload_ssh_server': None,
                'upload_remote_basepath': None,
                'enable_try_uploads': True,
            },
            all_actions=B2GHazardBuild.all_actions,
            default_actions=B2GHazardBuild.default_actions,
        )

        self.buildid = None
        self.analysis = HazardAnalysis()

    def _pre_config_lock(self, rw_config):
        super(B2GHazardBuild, self)._pre_config_lock(rw_config)

        if self.buildbot_config:
            self.config['is_automation'] = True
        else:
            self.config['is_automation'] = False

        dirs = self.query_abs_dirs()
        replacements = self.config['env_replacements'].copy()
        for k, v in replacements.items():
            replacements[k] = v % dirs

        self.env = self.query_env(replace_dict=replacements,
                                  partial_env=self.config['partial_env'],
                                  purge_env=nuisance_env_vars)

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(B2GHazardBuild, self).query_abs_dirs()

        abs_work_dir = abs_dirs['abs_work_dir']
        dirs = {
            'b2g_src':
                abs_work_dir,
            'target_compiler_base':
                os.path.join(abs_dirs['abs_work_dir'], 'target_compiler'),
            'shell_objdir':
                os.path.join(abs_work_dir, self.config['shell-objdir']),
            'mozharness_scriptdir':
                os.path.abspath(os.path.dirname(__file__)),
            'abs_analysis_dir':
                os.path.join(abs_work_dir, self.config['analysis-dir']),
            'abs_analyzed_objdir':
                os.path.join(abs_work_dir, self.config['srcdir'], self.config['analysis-objdir']),
            'analysis_scriptdir':
                os.path.join(abs_dirs['gecko_src'], self.config['analysis-scriptdir'])
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

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
        return os.path.join(dirs['analysis_scriptdir'], self.config['compiler_manifest'])

    def query_b2g_compiler_manifest(self):
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['analysis_scriptdir'], self.config['b2g_compiler_manifest'])

    def query_sixgill_manifest(self):
        dirs = self.query_abs_dirs()
        return os.path.join(dirs['analysis_scriptdir'], self.config['sixgill_manifest'])

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

    def query_upload_label(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['platform']
        else:
            return self.config.get('upload_label')

    def query_upload_path(self):
        branch = self.query_branch()

        common = {
            'basepath': self.query_upload_remote_basepath(),
            'branch': branch,
            'target': self.query_upload_label(),
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

    def make_source_dir(self):
        dirs = self.query_abs_dirs()
        dest = dirs['b2g_src']
        if not os.path.exists(dest):
            self.mkdir_p(dest)

    # The gecko config is required before enabling mock, because it determines
    # what mock target to use.
    def enable_mock(self):
        self.load_gecko_config()
        super(B2GHazardBuild, self).enable_mock()

    # Actions {{{2
    def clobber(self):
        dirs = self.query_abs_dirs()
        PurgeMixin.clobber(
            self,
            always_clobber_dirs=[
                dirs['abs_upload_dir'],
            ],
        )

    def checkout_sources(self):
        self.make_source_dir()
        super(B2GHazardBuild, self).checkout_sources()

    def get_blobs(self):
        dirs = self.query_abs_dirs()
        self.tooltool_fetch(self.query_compiler_manifest(), "sh " + self.config['compiler_setup'],
                            dirs['abs_work_dir'])
        self.tooltool_fetch(self.query_sixgill_manifest(), "sh " + self.config['sixgill_setup'],
                            dirs['abs_work_dir'])
        if not os.path.exists(dirs['target_compiler_base']):
            self.mkdir_p(dirs['target_compiler_base'])
        self.tooltool_fetch(self.query_b2g_compiler_manifest(), "sh " + self.config['compiler_setup'],
                            dirs['target_compiler_base'])

    def clobber_shell(self):
        dirs = self.query_abs_dirs()
        self.rmtree(dirs['shell_objdir'])

    def configure_shell(self):
        self.enable_mock()
        dirs = self.query_abs_dirs()

        if not os.path.exists(dirs['shell_objdir']):
            self.mkdir_p(dirs['shell_objdir'])

        js_src_dir = os.path.join(dirs['gecko_src'], 'js', 'src')
        rc = self.run_command(['autoconf-2.13'],
                              cwd=js_src_dir,
                              env=self.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            self.fatal("autoconf failed, can't continue.", exit_code=FAILURE)

        rc = self.run_command([os.path.join(js_src_dir, 'configure'),
                               '--enable-optimize',
                               '--disable-debug',
                               '--enable-ctypes',
                               '--with-system-nspr',
                               '--without-intl-api'],
                              cwd=dirs['shell_objdir'],
                              env=self.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            self.fatal("Configure failed, can't continue.", exit_code=FAILURE)

        self.disable_mock()

    def build_shell(self):
        self.enable_mock()
        dirs = self.query_abs_dirs()

        rc = self.run_command(['make', '-j', str(self.config.get('concurrency', 4)), '-s'],
                              cwd=dirs['shell_objdir'],
                              env=self.env,
                              error_list=MakefileErrorList)
        if rc != 0:
            self.fatal("Build failed, can't continue.", exit_code=FAILURE)

        self.disable_mock()

    def clobber_analysis(self):
        dirs = self.query_abs_dirs()
        self.rmtree(dirs['abs_analysis_dir'])
        self.rmtree(dirs['abs_analyzed_objdir'])

    def setup_analysis(self):
        dirs = self.query_abs_dirs()
        analysis_dir = dirs['abs_analysis_dir']

        if not os.path.exists(analysis_dir):
            self.mkdir_p(analysis_dir)

        values = {
            'js': os.path.join(dirs['shell_objdir'], 'dist', 'bin', 'js'),
            'analysis_scriptdir': dirs['analysis_scriptdir'],
            'source_objdir': dirs['abs_analyzed_objdir'],
            'source': os.path.join(dirs['work_dir'], 'source'),
            'sixgill': os.path.join(dirs['work_dir'], self.config['sixgill']),
            'sixgill_bin': os.path.join(dirs['work_dir'], self.config['sixgill_bin']),
        }
        defaults = """
js = '%(js)s'
analysis_scriptdir = '%(analysis_scriptdir)s'
objdir = '%(source_objdir)s'
source = '%(source)s'
sixgill = '%(sixgill)s'
sixgill_bin = '%(sixgill_bin)s'
jobs = 2
""" % values

        #defaults_path = os.path.join(analysis_dir, 'defaults.py')
        defaults_path = os.path.join(analysis_dir, 'defaults.py')
        file(defaults_path, "w").write(defaults)
        self.log("Wrote analysis config file " + defaults_path)

        build_command = self.config['build_command']
        self.copyfile(os.path.join(dirs['mozharness_scriptdir'],
                                   os.path.join('spidermonkey', build_command)),
                      os.path.join(analysis_dir, build_command),
                      copystat=True)

    def run_analysis(self):
        dirs = self.query_abs_dirs()

        env = self.query_build_env().copy()
        self.enable_mock()

        gonk_misc = os.path.join(dirs['b2g_src'], 'gonk-misc')
        mozconfig = os.path.join(gonk_misc, 'hazard-analysis-config')
        mozconfig_text = ''
        mozconfig_text += 'CXXFLAGS="-Wno-attributes"\n'
        mozconfig_text += '. "%s/default-gecko-config"\n' % gonk_misc
        basecc = os.path.join(dirs['abs_work_dir'], self.config['sixgill'], 'scripts', 'wrap_gcc', 'basecc')
        mozconfig_text += "ac_add_options --with-compiler-wrapper=" + basecc + "\n"
        mozconfig_text += "ac_add_options --without-ccache\n"
        file(mozconfig, "w").write(mozconfig_text)

        # Stuff I set in my .userconfig for manual builds
        env['B2G_SOURCE'] = dirs['b2g_src']
        env['MOZCONFIG_PATH'] = mozconfig
        env['GECKO_PATH'] = dirs['gecko_src']
        env['TARGET_TOOLS_PREFIX'] = os.path.join(dirs['abs_work_dir'], self.config['b2g_target_compiler_prefix'])

        try:
            self.analysis.run(self, env=env, error_list=B2GMakefileErrorList)
        except HazardError as e:
            self.fatal(e, exit_code=FAILURE)

        self.disable_mock()

    def collect_analysis_output(self):
        self.analysis.collect_output(self)

    def upload_analysis(self):
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
    myScript = B2GHazardBuild()
    myScript.run_and_exit()
