#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****

import sys
import os
import glob
import re
import tempfile
from datetime import datetime
import urlparse
import multiprocessing
import xml.dom.minidom

try:
    import simplejson as json
    assert json
except ImportError:
    import json

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.config import parse_config_file
from mozharness.base.log import WARNING, FATAL
from mozharness.mozilla.l10n.locales import GaiaLocalesMixin, LocalesMixin
from mozharness.mozilla.purge import PurgeMixin
from mozharness.mozilla.signing import SigningMixin
from mozharness.mozilla.repo_manifest import add_project
from mozharness.mozilla.mapper import MapperMixin
from mozharness.mozilla.updates.balrog import BalrogMixin
from mozharness.base.python import VirtualenvMixin
from mozharness.base.python import InfluxRecordingMixin
from mozharness.mozilla.building.buildbase import MakeUploadOutputParser
from mozharness.mozilla.building.buildb2gbase import B2GBuildBaseScript, B2GMakefileErrorList
from mozharness.base.script import PostScriptRun


class B2GBuild(LocalesMixin, PurgeMixin,
               B2GBuildBaseScript,
               GaiaLocalesMixin, SigningMixin, MapperMixin, BalrogMixin,
               VirtualenvMixin, InfluxRecordingMixin):
    all_actions = [
        'clobber',
        'checkout-sources',
        # Deprecated
        'checkout-gecko',
        'download-gonk',
        'unpack-gonk',
        'checkout-gaia',
        'checkout-gaia-l10n',
        'checkout-gecko-l10n',
        # End deprecated
        'get-blobs',
        'update-source-manifest',
        'build',
        'build-symbols',
        'make-updates',
        'build-update-testdata',
        'prep-upload',
        'upload',
        'make-socorro-json',
        'upload-source-manifest',
        'submit-to-balrog',
    ]

    default_actions = [
        'checkout-sources',
        'get-blobs',
        'build',
    ]

    config_options = [
        [["--gaia-languages-file"], {
            "dest": "gaia_languages_file",
            "help": "languages file for gaia multilocale profile",
        }],
        [["--gecko-languages-file"], {
            "dest": "locales_file",
            "help": "languages file for gecko multilocale",
        }],
        [["--gecko-l10n-base-dir"], {
            "dest": "l10n_dir",
            "help": "dir to clone gecko l10n repos into, relative to the work directory",
        }],
        [["--merge-locales"], {
            "dest": "merge_locales",
            "help": "Dummy option to keep from burning. We now always merge",
        }],
        [["--additional-source-tarballs"], {
            "action": "extend",
            "type": "string",
            "dest": "additional_source_tarballs",
            "help": "Additional source tarballs to extract",
        }],
        [["--debug"], {
            "dest": "debug_build",
            "action": "store_true",
            "help": "Set B2G_DEBUG=1 (debug build)",
        }],
        [["--base-repo"], {
            "dest": "base_repo",
            "help": "base repository for cloning",
        }],
        [["--complete-mar-url"], {
            "dest": "complete_mar_url",
            "help": "the URL where the complete MAR was uploaded. Required if submit-to-balrog is requested and upload isn't.",
        }],
        [["--platform"], {
            "dest": "platform",
            "help": "the platform used by balrog submmiter.",
        }],
        [["--gecko-objdir"], {
            "dest": "gecko_objdir",
            "help": "Specifies the gecko object directory.",
        }],
        [["--branch"], {
            "dest": "branch",
            "help": "Specifies the branch name.",
        }],
    ]

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):
        # Default configuration
        default_config = {
            'default_vcs': 'hg',
            'ccache': True,
            'locales_dir': 'gecko/b2g/locales',
            'l10n_dir': 'gecko-l10n',
            'ignore_locales': ['en-US', 'multi'],
            'locales_file': 'gecko/b2g/locales/all-locales',
            'mozilla_dir': 'build/gecko',
            'objdir': 'build/objdir-gecko',
            'merge_locales': True,
            'repo_remote_mappings': {},
            'influx_credentials_file': 'oauth.txt',
            'balrog_credentials_file': 'oauth.txt',
            'build_resources_path': '%(abs_obj_dir)s/.mozbuild/build_resources.json',
            'virtualenv_modules': [
                'requests==2.8.1',
            ],
            'virtualenv_path': 'venv',
        }
        default_config.update(config)

        self.buildid = None
        self.dotconfig = None
        LocalesMixin.__init__(self)
        B2GBuildBaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions,
        )

        dirs = self.query_abs_dirs()
        self.objdir = self.config.get("gecko_objdir",
                os.path.join(dirs['work_dir'], 'objdir-gecko'))
        self.abs_dirs['abs_obj_dir'] = self.objdir

        # Evaluating the update type to build.
        # Default is OTA if config do not specifies anything
        if "update_types" in self.config:
            self.update_types = self.config["update_types"]
        elif "update_type" in self.config:
            self.update_types = [self.config["update_type"]]
        else:
            self.update_types = ["ota"]

        self.package_urls = {}

        # We need to create the virtualenv directly (without using an action) in
        # order to use python modules in PreScriptRun/Action listeners
        self.create_virtualenv()

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = LocalesMixin.query_abs_dirs(self)
        abs_dirs.update(B2GBuildBaseScript.query_abs_dirs(self))

        dirs = {
            'gaia_l10n_base_dir': os.path.join(abs_dirs['abs_work_dir'], 'gaia-l10n'),
            'abs_public_upload_dir': os.path.join(abs_dirs['abs_work_dir'], 'upload-public'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def query_branch(self):
        if self.buildbot_config and 'properties' in self.buildbot_config:
            return self.buildbot_config['properties']['branch']
        else:
            return self.config.get('branch', os.path.basename(self.query_repo().rstrip('/')))

    def query_buildid(self):
        if self.buildid:
            return self.buildid
        platform_ini = os.path.join(self.query_device_outputdir(),
                                    'system', 'b2g', 'platform.ini')
        data = self.read_from_file(platform_ini)
        buildid = re.search("^BuildID=(\d+)$", data, re.M)
        if buildid:
            self.buildid = buildid.group(1)
            return self.buildid

    def query_version(self):
        data = self.read_from_file(self.query_application_ini())
        version = re.search("^Version=(.+)$", data, re.M)
        if version:
            return version.group(1)

    def query_b2g_version(self):
        manifest_config = self.config.get('manifest')
        branch = self.query_branch()
        if not manifest_config or not branch:
            return 'default'
        if branch not in manifest_config['branches']:
            return 'default'
        version = manifest_config['branches'][branch]
        return version

    def query_update_channel(self):
        env = self.query_env()
        if 'B2G_UPDATE_CHANNEL' in env:
            return env['B2G_UPDATE_CHANNEL']

    def get_hg_commit_time(self, repo_dir, rev):
        """Returns the commit time for given `rev` in unix epoch time"""
        hg = self.query_exe('hg')
        cmd = [
            hg,
            'log',
            '-R', repo_dir,
            '-r', rev,
            '--template', '{date|hgdate}'
        ]
        try:
            # {date|hgdate} returns a space-separated tuple of unixtime,
            # timezone offset
            output = self.get_output_from_command(cmd)
        except Exception:
            # Failed to run hg for some reason
            self.exception("failed to run hg log; using timestamp of 0 instead", level=WARNING)
            return 0

        try:
            t = output.split()[0]
            return int(t)
        except (ValueError, IndexError):
            self.exception("failed to parse hg log output; using timestamp of 0 instead", level=WARNING)
            return 0

    def query_do_upload(self):
        # always upload nightlies, but not dep builds for some platforms
        if self.query_is_nightly():
            return True
        if self.config['target'] in self.config['upload']['default'].get('upload_dep_target_exclusions', []):
            return False
        return True

    def query_build_env(self):
        env = super(B2GBuild, self).query_build_env()

        # Force B2G_UPDATER so that eng builds (like the emulator) will get
        # the updater included. Otherwise the xpcshell updater tests won't run.
        # Bug 1154947: we don't want nightly builds for eng phones, but nightly
        # builds fail if we don't have OTA updates.
        if self.query_is_nightly() or self.config['target'].startswith('emulator'):
            env['B2G_UPDATER'] = '1'
        # Bug 1059992 -- see gonk-misc/Android.mk
        env['FORCE_GECKO_BUILD_OUTPUT'] = '1'
        if self.config.get('debug_build'):
            env['B2G_DEBUG'] = '1'
        return env

    def query_dotconfig(self):
        if self.dotconfig:
            return self.dotconfig
        dirs = self.query_abs_dirs()
        dotconfig_file = os.path.join(dirs['abs_work_dir'], '.config')
        self.dotconfig = {}
        for line in open(dotconfig_file):
            if "=" in line:
                key, value = line.split("=", 1)
                self.dotconfig[key.strip()] = value.strip()
        return self.dotconfig

    def query_device_outputdir(self):
        dirs = self.query_abs_dirs()
        dotconfig = self.query_dotconfig()
        if 'DEVICE' in dotconfig:
            devicedir = dotconfig['DEVICE']
        elif 'PRODUCT_NAME' in dotconfig:
            devicedir = dotconfig['PRODUCT_NAME']
        else:
            self.fatal("Couldn't determine device directory")
        output_dir = os.path.join(dirs['work_dir'], 'out', 'target', 'product', devicedir)
        return output_dir

    def query_device_name(self):
        return os.path.basename(self.query_device_outputdir())

    def query_application_ini(self):
        return os.path.join(self.query_device_outputdir(), 'system', 'b2g', 'application.ini')

    def query_marfile_path(self, update_type):
        if update_type.startswith("fota"):
            mardir = self.query_device_outputdir()
        else:
            mardir = "%s/dist/b2g-update" % self.objdir

        device_name = self.query_device_name()
        update_type_files = {
            'ota':          'b2g-%s-gecko-update.mar'    % device_name,
            'fota':         'fota-%s-update.mar'         % device_name,
            'fota:full':    'fota-%s-update-full.mar'    % device_name,
            'fota:fullimg': 'fota-%s-update-fullimg.mar' % device_name
        }

        mars = []
        for f in os.listdir(mardir):
            if f.endswith(update_type_files[update_type]):
                mars.append(f)

        if len(mars) < 1:
            self.fatal("Found none or too many marfiles in %s, don't know what to do:\n%s" % (mardir, mars), exit_code=1)

        return "%s/%s" % (mardir, mars[0])

    def query_complete_mar_url(self, marfile=None):
        mar_url = None
        if "complete_mar_url" in self.config:
            mar_url = self.config["complete_mar_url"]
        elif "completeMarUrl" in self.package_urls:
            mar_url = self.package_urls["completeMarUrl"]
        else:
            self.fatal("Couldn't find complete mar url in config or package_urls")

        # To support OTA and FOTA update payload, we cannot rely on the filename
        # being computed before we get called since we will determine the filename
        # ourselves. Let's detect when the URL does not ends with ".mar" and in
        # this case, we append the MAR file we just collected
        if not mar_url.endswith(".mar"):
            if marfile is None:
                self.fatal("URL does not contains a MAR file and none found")
            mar_url = os.path.join(mar_url, os.path.basename(marfile))

        return mar_url

    # Actions {{{2
    def clobber(self):
        dirs = self.query_abs_dirs()
        PurgeMixin.clobber(
            self,
            always_clobber_dirs=[
                dirs['abs_upload_dir'],
                dirs['abs_public_upload_dir'],
            ],
        )

    def checkout_sources(self):
        super(B2GBuild, self).checkout_sources()
        self.checkout_gecko_l10n()
        self.checkout_gaia_l10n()

    def get_blobs(self):
        self.download_blobs()
        self.unpack_blobs()

    def download_blobs(self):
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        if 'tooltool_manifest' in gecko_config:
            # The manifest is relative to the gecko config
            config_dir = os.path.join(dirs['gecko_src'], 'b2g', 'config',
                                      self.config.get('b2g_config_dir', self.config['target']))
            manifest = os.path.abspath(os.path.join(config_dir, gecko_config['tooltool_manifest']))
            self.tooltool_fetch(manifest=manifest, output_dir=dirs['gecko_src'])

    def unpack_blobs(self):
        dirs = self.query_abs_dirs()
        tar = self.query_exe('tar', return_type="list")
        gecko_config = self.load_gecko_config()
        extra_tarballs = self.config.get('additional_source_tarballs', [])
        if 'additional_source_tarballs' in gecko_config:
            extra_tarballs.extend(gecko_config['additional_source_tarballs'])

        for tarball in extra_tarballs:
            self.run_command(tar + ["xf", tarball], cwd=dirs['gecko_src'],
                             halt_on_failure=True, fatal_exit_code=3)

    def checkout_gaia_l10n(self):
        if not self.config.get('gaia_languages_file'):
            self.info('Skipping checkout_gaia_l10n because no gaia language file was specified.')
            return

        l10n_config = self.load_gecko_config().get('gaia', {}).get('l10n')
        if not l10n_config:
            self.fatal("gaia.l10n is required in the gecko config when --gaia-languages-file is specified.")

        abs_work_dir = self.query_abs_dirs()['abs_work_dir']
        languages_file = os.path.join(abs_work_dir, 'gaia', self.config['gaia_languages_file'])
        l10n_base_dir = self.query_abs_dirs()['gaia_l10n_base_dir']

        self.pull_gaia_locale_source(l10n_config, parse_config_file(languages_file).keys(), l10n_base_dir)

    def checkout_gecko_l10n(self):
        hg_l10n_base = self.load_gecko_config().get('gecko_l10n_root')
        self.pull_locale_source(hg_l10n_base=hg_l10n_base)
        gecko_locales = self.query_locales()
        # populate b2g/overrides, which isn't in gecko atm
        dirs = self.query_abs_dirs()
        for locale in gecko_locales:
            self.mkdir_p(os.path.join(dirs['abs_l10n_dir'], locale, 'b2g', 'chrome', 'overrides'))
            self.copytree(os.path.join(dirs['abs_l10n_dir'], locale, 'mobile', 'overrides'),
                          os.path.join(dirs['abs_l10n_dir'], locale, 'b2g', 'chrome', 'overrides'),
                          error_level=FATAL)

    def query_do_translate_hg_to_git(self, gecko_config_key=None):
        manifest_config = self.config.get('manifest', {})
        branch = self.query_branch()
        if self.query_is_nightly() and branch in manifest_config['branches'] and \
                manifest_config.get('translate_hg_to_git'):
            if gecko_config_key is None:
                return True
            if self.gecko_config.get(gecko_config_key):
                return True
        return False

    def _generate_git_locale_manifest(self, locale, url, git_repo,
                                      revision, git_base_url, local_path):
        # increase timeout from 15m to 60m until bug 1044515 is resolved (attempts = 120)
        l10n_git_sha = self.query_mapper_git_revision(url, 'l10n', revision, project_name="l10n %s" % locale,
                                                      require_answer=self.config.get('require_git_rev', True), attempts=120)
        return '  <project name="%s" path="%s" remote="mozillaorg" revision="%s"/>' % (git_repo.replace(git_base_url, ''), local_path, l10n_git_sha)

    def _generate_locale_manifest(self, git_base_url="https://git.mozilla.org/release/"):
        """ Add the locales to the source manifest.
        """
        manifest_config = self.config.get('manifest', {})
        locale_manifest = []
        if self.gaia_locale_revisions:
            gaia_l10n_git_root = None
            if self.query_do_translate_hg_to_git(gecko_config_key='gaia_l10n_git_root'):
                gaia_l10n_git_root = self.gecko_config['gaia_l10n_git_root']
            for locale in self.gaia_locale_revisions.keys():
                repo = self.gaia_locale_revisions[locale]['repo']
                revision = self.gaia_locale_revisions[locale]['revision']
                locale_manifest.append('  <!-- Mercurial-Information: <project name="%s" path="gaia-l10n/%s" remote="hgmozillaorg" revision="%s"/> -->' %
                                       (repo.replace('https://hg.mozilla.org/', ''), locale, revision))
                if gaia_l10n_git_root:
                    locale_manifest.append(
                        self._generate_git_locale_manifest(
                            locale,
                            manifest_config['translate_base_url'],
                            gaia_l10n_git_root % {'locale': locale},
                            revision,
                            git_base_url,
                            "gaia-l10n/%s" % locale,
                        )
                    )
        if self.gecko_locale_revisions:
            gecko_l10n_git_root = None
            if self.query_do_translate_hg_to_git(gecko_config_key='gecko_l10n_git_root'):
                gecko_l10n_git_root = self.gecko_config['gecko_l10n_git_root']
            for locale in self.gecko_locale_revisions.keys():
                repo = self.gecko_locale_revisions[locale]['repo']
                revision = self.gecko_locale_revisions[locale]['revision']
                locale_manifest.append('  <!-- Mercurial-Information: <project name="%s" path="gecko-l10n/%s" remote="hgmozillaorg" revision="%s"/> -->' %
                                       (repo.replace('https://hg.mozilla.org/', ''), locale, revision))
                if gecko_l10n_git_root:
                    locale_manifest.append(
                        self._generate_git_locale_manifest(
                            locale,
                            manifest_config['translate_base_url'],
                            gecko_l10n_git_root % {'locale': locale},
                            revision,
                            git_base_url,
                            "gecko-l10n/%s" % locale,
                        )
                    )
        return locale_manifest

    def update_source_manifest(self):
        dirs = self.query_abs_dirs()
        manifest_config = self.config.get('manifest', {})

        sourcesfile = os.path.join(dirs['work_dir'], 'sources.xml')
        sourcesfile_orig = sourcesfile + '.original'
        sources = self.read_from_file(sourcesfile_orig, verbose=False)
        dom = xml.dom.minidom.parseString(sources)
        # Add comments for which hg revisions we came from
        manifest = dom.firstChild
        manifest.appendChild(dom.createTextNode("  "))
        manifest.appendChild(dom.createComment("Mozilla Info"))
        manifest.appendChild(dom.createTextNode("\n  "))
        manifest.appendChild(dom.createComment('Mercurial-Information: <remote fetch="https://hg.mozilla.org/" name="hgmozillaorg">'))
        manifest.appendChild(dom.createTextNode("\n  "))
        manifest.appendChild(dom.createComment('Mercurial-Information: <project name="%s" path="gecko" remote="hgmozillaorg" revision="%s"/>' %
                             (self.query_repo(), self.query_revision())))

        if self.query_do_translate_hg_to_git():
            # Find the base url used for git.m.o so we can refer to it
            # properly in the project node below
            git_base_url = "https://git.mozilla.org/"
            for element in dom.getElementsByTagName('remote'):
                if element.getAttribute('name') == 'mozillaorg':
                    pieces = urlparse.urlparse(element.getAttribute('fetch'))
                    if pieces:
                        git_base_url = "https://git.mozilla.org%s" % pieces[2]
                        if not git_base_url.endswith('/'):
                            git_base_url += "/"
                        self.info("Found git_base_url of %s in manifest." % git_base_url)
                        break
            else:
                self.warning("Couldn't find git_base_url in manifest; using %s" % git_base_url)

            manifest.appendChild(dom.createTextNode("\n  "))
            url = manifest_config['translate_base_url']
            # increase timeout from 15m to 60m until bug 1044515 is resolved (attempts = 120)
            gecko_git = self.query_mapper_git_revision(url, 'gecko',
                                                       self.query_revision(),
                                                       require_answer=self.config.get('require_git_rev',
                                                                                      True),
                                                       attempts=120)
            project_name = "https://git.mozilla.org/releases/gecko.git".replace(git_base_url, '')
            # XXX This assumes that we have a mozillaorg remote
            add_project(dom, name=project_name, path="gecko", remote="mozillaorg", revision=gecko_git)
        manifest.appendChild(dom.createTextNode("\n"))

        self.write_to_file(sourcesfile, dom.toxml(), verbose=False)
        self.run_command(["diff", "-u", sourcesfile_orig, sourcesfile], success_codes=[1])

    def generate_build_command(self, target=None):
        cmd = ['./build.sh']
        if target is not None:
            # Workaround bug 984061
            # wcosta: blobfree builds also should run with -j1
            if target in ('package-tests', 'blobfree'):
                cmd.append('-j1')
            else:
                # Ensure we always utilize the correct number of cores
                # regardless of the configuration which may be set by repo
                # config changes...
                cmd.append('-j{0}'.format(multiprocessing.cpu_count()))
            cmd.append(target)
        return cmd

    def build(self):
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        build_targets = gecko_config.get('build_targets', [])
        build_targets.extend(self.config.get("build_targets", []))
        if not build_targets:
            cmds = [self.generate_build_command()]
        else:
            cmds = [self.generate_build_command(t) for t in build_targets]

        env = self.query_build_env()
        if self.config.get('gaia_languages_file'):
            env['LOCALE_BASEDIR'] = dirs['gaia_l10n_base_dir']
            env['LOCALES_FILE'] = os.path.join(dirs['abs_work_dir'], 'gaia', self.config['gaia_languages_file'])
        if self.config.get('locales_file'):
            env['L10NBASEDIR'] = dirs['abs_l10n_dir']
            env['MOZ_CHROME_MULTILOCALE'] = " ".join(self.query_locales())
            if 'PATH' not in env:
                env['PATH'] = os.environ.get('PATH')
            env['PYTHONPATH'] = os.environ.get('PYTHONPATH', '')

        with open(os.path.join(dirs['work_dir'], '.userconfig'), 'w') as cfg:
            cfg.write('GECKO_OBJDIR={0}'.format(self.objdir))

        self.enable_mock()
        if self.config['ccache']:
            self.run_command('ccache -z', cwd=dirs['work_dir'], env=env)
        for cmd in cmds:
            retval = self.run_command(cmd, cwd=dirs['work_dir'], env=env, error_list=B2GMakefileErrorList)
            if retval != 0:
                break
        if self.config['ccache']:
            self.run_command('ccache -s', cwd=dirs['work_dir'], env=env)
        self.disable_mock()

        if retval != 0:
            self.fatal("failed to build", exit_code=2)

        buildid = self.query_buildid()
        self.set_buildbot_property('buildid', buildid, write_to_file=True)

    def build_symbols(self):
        dirs = self.query_abs_dirs()
        gecko_config = self.load_gecko_config()
        if gecko_config.get('config_version', 0) < 1:
            self.info("Skipping build_symbols for old configuration")
            return

        cmd = ['./build.sh', 'buildsymbols']
        env = self.query_build_env()

        self.enable_mock()
        retval = self.run_command(cmd, cwd=dirs['work_dir'], env=env, error_list=B2GMakefileErrorList)
        self.disable_mock()

        if retval != 0:
            self.fatal("failed to build symbols", exit_code=2)

        if self.query_is_nightly():
            # Upload symbols
            self.info("Uploading symbols")
            cmd = ['./build.sh', 'uploadsymbols']
            self.enable_mock()
            retval = self.run_command(cmd, cwd=dirs['work_dir'], env=env, error_list=B2GMakefileErrorList)
            self.disable_mock()

            if retval != 0:
                self.fatal("failed to upload symbols", exit_code=2)

    def make_updates(self):
        if not self.update_types:
            self.fatal("No update types defined. We should have had at least defaulted to OTA ...")

        dirs = self.query_abs_dirs()

        self.load_gecko_config()

        env = self.query_build_env()

        for update_type in self.update_types:
            # Defaulting to OTA
            make_target = "gecko-update-full"

            # Building a FOTA with only Gecko/Gaia (+ a few redistribuable)
            if update_type == "fota":
                make_target = "gecko-update-fota"

            # Building a FOTA with all system partition files
            if update_type == "fota:full":
                make_target = "gecko-update-fota-full"

            # Building a FOTA with full partitions images
            if update_type == "fota:fullimg":
                make_target = "gecko-update-fota-fullimg"

            cmd = ['./build.sh', make_target]

            self.enable_mock()
            retval = self.run_command(cmd, cwd=dirs['work_dir'], env=env, error_list=B2GMakefileErrorList)
            self.disable_mock()

            if retval != 0:
                self.fatal("failed to create update", exit_code=2)

    def prep_upload(self):
        if not self.query_do_upload():
            self.info("Uploads disabled for this build. Skipping...")
            return

        dirs = self.query_abs_dirs()

        # Copy stuff into build/upload directory
        gecko_config = self.load_gecko_config()

        output_dir = self.query_device_outputdir()

        # Zip up stuff
        files = []
        for item in gecko_config.get('zip_files', []):
            if isinstance(item, list):
                pattern, target = item
            else:
                pattern, target = item, None

            pattern = pattern.format(objdir=self.objdir, workdir=dirs['work_dir'], srcdir=dirs['gecko_src'])
            for f in glob.glob(pattern):
                files.append((f, target))

        if files:
            zip_name = os.path.join(dirs['work_dir'], self.config['target'] + ".zip")
            self.info("creating %s" % zip_name)
            tmpdir = tempfile.mkdtemp()
            try:
                zip_dir = os.path.join(tmpdir, 'b2g-distro')
                self.mkdir_p(zip_dir)
                for f, target in files:
                    if target is None:
                        dst = os.path.join(zip_dir, os.path.basename(f))
                    elif target.endswith('/'):
                        dst = os.path.join(zip_dir, target, os.path.basename(f))
                    else:
                        dst = os.path.join(zip_dir, target)
                    if not os.path.exists(os.path.dirname(dst)):
                        self.mkdir_p(os.path.dirname(dst))
                    self.copyfile(f, dst, copystat=True)

                cmd = ['zip', '-r', '-9', '-u', zip_name, 'b2g-distro']
                if self.run_command(cmd, cwd=tmpdir) != 0:
                    self.fatal("problem zipping up files")
                self.copy_to_upload_dir(zip_name)
            finally:
                self.debug("removing %s" % tmpdir)
                self.rmtree(tmpdir)

        public_files = []
        public_upload_patterns = []
        public_upload_patterns = gecko_config.get('public_upload_files', [])
        # Copy gaia profile
        if gecko_config.get('package_gaia', True):
            zip_name = os.path.join(dirs['work_dir'], "gaia.zip")
            self.info("creating %s" % zip_name)
            cmd = ['zip', '-r', '-9', '-u', zip_name, 'gaia/profile']
            if self.run_command(cmd, cwd=dirs['work_dir']) != 0:
                self.fatal("problem zipping up gaia")
            self.copy_to_upload_dir(zip_name)
            if public_upload_patterns:
                public_files.append(zip_name)

        self.info("copying files to upload directory")
        files = []

        files.append(os.path.join(output_dir, 'system', 'build.prop'))

        upload_patterns = gecko_config.get('upload_files', [])
        for base_pattern in upload_patterns + public_upload_patterns:
            pattern = base_pattern.format(objdir=self.objdir, workdir=dirs['work_dir'], srcdir=dirs['gecko_src'])
            for f in glob.glob(pattern):
                if base_pattern in upload_patterns:
                    files.append(f)
                if base_pattern in public_upload_patterns:
                    public_files.append(f)

        blobfree_dist = self.query_device_name() + '.blobfree-dist.zip'
        blobfree_zip  = os.path.join(output_dir, blobfree_dist)

        if os.path.exists(blobfree_zip):
            public_files.append(blobfree_zip)

        for base_f in files + public_files:
            f = base_f
            if f.endswith(".img"):
                if self.query_is_nightly():
                    # Compress it
                    if os.path.exists(f):
                        self.info("compressing %s" % f)
                        self.run_command(["bzip2", "-f", f])
                    elif not os.path.exists("%s.bz2" % f):
                        self.error("%s doesn't exist to bzip2!" % f)
                        self.return_code = 2
                        continue
                    f = "%s.bz2" % base_f
                else:
                    # Skip it
                    self.info("not uploading %s for non-nightly build" % f)
                    continue
            if base_f in files:
                self.info("copying %s to upload directory" % f)
                self.copy_to_upload_dir(f)
            if base_f in public_files:
                self.info("copying %s to public upload directory" % f)
                self.copy_to_upload_dir(base_f, upload_dir=dirs['abs_public_upload_dir'])

    def _do_rsync_upload(self, upload_dir, ssh_key, ssh_user, remote_host,
                         remote_path, remote_symlink_path):
        retval = self.rsync_upload_directory(upload_dir, ssh_key, ssh_user,
                                             remote_host, remote_path)
        if retval is not None:
            self.error("Failed to upload %s to %s@%s:%s!" % (upload_dir, ssh_user, remote_host, remote_path))
            self.return_code = 2
            return -1
        upload_url = "http://%(remote_host)s/%(remote_path)s" % dict(
            remote_host=remote_host,
            remote_path=remote_path,
        )
        self.info("Upload successful: %s" % upload_url)

        if remote_symlink_path:
            ssh = self.query_exe('ssh')
            # First delete the symlink if it exists
            cmd = [ssh,
                   '-l', ssh_user,
                   '-i', ssh_key,
                   remote_host,
                   'rm -f %s' % remote_symlink_path,
                   ]
            retval = self.run_command(cmd)
            if retval != 0:
                self.error("failed to delete latest symlink")
                self.return_code = 2
            # Now create the symlink
            rel_path = os.path.relpath(remote_path, os.path.dirname(remote_symlink_path))
            cmd = [ssh,
                   '-l', ssh_user,
                   '-i', ssh_key,
                   remote_host,
                   'ln -sf %s %s' % (rel_path, remote_symlink_path),
                   ]
            retval = self.run_command(cmd)
            if retval != 0:
                self.error("failed to create latest symlink")
                self.return_code = 2
            # Create properties file
            remote_properties_path = '%s.properties' % remote_symlink_path
            cmd = [ssh,
                   '-l', ssh_user,
                   '-i', ssh_key,
                   remote_host,
                   'echo B2G_BUILD_PATH=%s > %s' % (remote_path,
                                                    remote_properties_path),
                   ]
            retval = self.run_command(cmd)
            if retval != 0:
                self.error("failed to create properties file")
                self.return_code = 2

    def _do_postupload_upload(self, upload_dir, ssh_key, ssh_user, remote_host,
                              postupload_cmd):
        ssh = self.query_exe('ssh')
        remote_path = self.get_output_from_command(
            [ssh, '-l', ssh_user, '-i', ssh_key, remote_host, 'mktemp -d']
        )
        if not remote_path.endswith('/'):
            remote_path += '/'
        retval = self.rsync_upload_directory(upload_dir, ssh_key, ssh_user,
                                             remote_host, remote_path)
        if retval is not None:
            self.error("Failed to upload %s to %s@%s:%s!" % (upload_dir, ssh_user, remote_host, remote_path))
            self.return_code = 2
        else:  # post_upload.py
            parser = MakeUploadOutputParser(
                config=self.config,
                log_obj=self.log_obj
            )
            # build filelist
            filelist = []
            for dirpath, dirname, filenames in os.walk(upload_dir):
                for f in filenames:
                    # use / instead of os.path.join() because this is part of
                    # a post_upload.py call on a fileserver, which is probably
                    # not windows
                    path = '%s/%s' % (dirpath, f)
                    path = path.replace(upload_dir, remote_path)
                    filelist.append(path)
            cmd = [ssh,
                   '-l', ssh_user,
                   '-i', ssh_key,
                   remote_host,
                   '%s %s %s' % (postupload_cmd, remote_path, ' '.join(filelist))
                   ]
            retval = self.run_command(cmd, output_parser=parser)
            self.package_urls = parser.matches
            if retval != 0:
                self.error("failed to run %s!" % postupload_cmd)
                self.return_code = 2
            else:
                self.info("Upload successful.")
        # cleanup, whether we ran postupload or not
        cmd = [ssh,
               '-l', ssh_user,
               '-i', ssh_key,
               remote_host,
               'rm -rf %s' % remote_path
               ]
        self.run_command(cmd)

    def upload(self):
        if not self.query_do_upload():
            self.info("Uploads disabled for this build. Skipping...")
            return

        dirs = self.query_abs_dirs()
        c = self.config
        target = self.load_gecko_config().get('upload_platform', self.config['target'])
        if c.get("target_suffix"):
            target += c["target_suffix"]
        if self.config.get('debug_build'):
            target += "-debug"
        try:
            # for Try
            user = self.buildbot_config['sourcestamp']['changes'][0]['who']
        except (TypeError, KeyError, IndexError):
            user = "unknown"

        replace_dict = dict(
            branch=self.query_branch(),
            target=target,
            user=user,
            revision=self.query_revision(),
            buildid=self.query_buildid(),
        )
        upload_path_key = 'upload_remote_path'
        upload_symlink_key = 'upload_remote_symlink'
        postupload_key = 'post_upload_cmd'
        if self.query_is_nightly():
            # Dates should be based on buildid
            build_date = self.query_buildid()
            if build_date:
                try:
                    build_date = datetime.strptime(build_date, "%Y%m%d%H%M%S")
                except ValueError:
                    build_date = None
            if build_date is None:
                # Default to now
                build_date = datetime.now()
            replace_dict.update(dict(
                year=build_date.year,
                month=build_date.month,
                day=build_date.day,
                hour=build_date.hour,
                minute=build_date.minute,
                second=build_date.second,
            ))
            upload_path_key = 'upload_remote_nightly_path'
            upload_symlink_key = 'upload_remote_nightly_symlink'
            postupload_key = 'post_upload_nightly_cmd'

        # default upload
        upload_path = self.config['upload']['default'][upload_path_key] % replace_dict
        if not self._do_rsync_upload(
            dirs['abs_upload_dir'],
            self.config['upload']['default']['ssh_key'],
            self.config['upload']['default']['ssh_user'],
            self.config['upload']['default']['upload_remote_host'],
            upload_path,
            self.config['upload']['default'].get(upload_symlink_key, '') % replace_dict,
        ):  # successful; sendchange
            # TODO unhardcode
            download_url = "http://pvtbuilds.pvt.build.mozilla.org%s" % upload_path

            if self.config["target"].startswith("emulator") and self.config.get('sendchange_masters'):
                # yay hardcodes
                downloadables = [
                    '%s/%s' % (download_url, 'emulator.tar.gz'),
                ]
                self.set_buildbot_property('packageUrl', downloadables[0], write_to_file=True)
                matches = glob.glob(os.path.join(dirs['abs_upload_dir'], 'b2g*crashreporter-symbols.zip'))
                if matches:
                    symbols_url = "%s/%s" % (download_url, os.path.basename(matches[0]))
                    downloadables.append(symbols_url)
                    self.set_buildbot_property('symbolsUrl', symbols_url, write_to_file=True)
                matches = glob.glob(os.path.join(dirs['abs_upload_dir'], 'b2g*test_packages.json'))
                if matches:
                    test_packages_url = "%s/%s" % (download_url, os.path.basename(matches[0]))
                    downloadables.append(test_packages_url)
                    self.set_buildbot_property('testPackagesUrl', test_packages_url,
                                               write_to_file=True)
                    self.invoke_sendchange(downloadables=downloadables)

        if self.query_is_nightly() and os.path.exists(dirs['abs_public_upload_dir']) and self.config['upload'].get('public'):
            self.info("Uploading public bits...")
            self._do_postupload_upload(
                dirs['abs_public_upload_dir'],
                self.config['upload']['public']['ssh_key'],
                self.config['upload']['public']['ssh_user'],
                self.config['upload']['public']['upload_remote_host'],
                self.config['upload']['public'][postupload_key] % replace_dict,
            )

    def make_socorro_json(self):
        self.info("Creating socorro.json...")
        dirs = self.query_abs_dirs()
        socorro_dict = {
            'buildid': self.query_buildid(),
            'version': self.query_version(),
            'update_channel': self.query_update_channel(),
            #'beta_number': n/a until we build b2g beta releases
        }
        file_path = os.path.join(dirs['abs_work_dir'], 'socorro.json')
        fh = open(file_path, 'w')
        json.dump(socorro_dict, fh)
        fh.close()
        self.run_command(["cat", file_path])

    def upload_source_manifest(self):
        manifest_config = self.config.get('manifest')
        branch = self.query_branch()
        if not manifest_config or not branch:
            self.info("No manifest config or can't get branch from build. Skipping...")
            return
        if branch not in manifest_config['branches']:
            self.info("Manifest upload not enabled for this branch. Skipping...")
            return
        dirs = self.query_abs_dirs()
        upload_dir = dirs['abs_upload_dir'] + '-manifest'
        # Delete the upload dir so we don't upload previous stuff by accident
        self.rmtree(upload_dir)
        target = self.load_gecko_config().get('upload_platform', self.config['target'])
        if self.config['manifest'].get('target_suffix'):
            target += self.config['manifest']['target_suffix']
        buildid = self.query_buildid()

        if self.query_is_nightly():
            version = manifest_config['branches'][branch]
            upload_remote_basepath = self.config['manifest']['upload_remote_basepath'] % {'version': version}
            # Dates should be based on buildid
            if buildid:
                try:
                    buildid = datetime.strptime(buildid, "%Y%m%d%H%M%S")
                except ValueError:
                    buildid = None
            if buildid is None:
                # Default to now
                buildid = datetime.now()
            # emulator builds will disappear out of latest/ because they're once-daily
            date_string = '%(year)04i-%(month)02i-%(day)02i-%(hour)02i' % dict(
                year=buildid.year,
                month=buildid.month,
                day=buildid.day,
                hour=buildid.hour,
            )
            xmlfilename = 'source_%(target)s_%(date_string)s.xml' % dict(
                target=target,
                date_string=date_string,
            )
            socorro_json = os.path.join(dirs['work_dir'], 'socorro.json')
            socorro_filename = 'socorro_%(target)s_%(date_string)s.json' % dict(
                target=target,
                date_string=date_string,
            )
            if os.path.exists(socorro_json):
                self.copy_to_upload_dir(
                    socorro_json,
                    os.path.join(upload_dir, socorro_filename)
                )
            tbpl_string = None
        else:
            upload_remote_basepath = self.config['manifest']['depend_upload_remote_basepath'] % {
                'branch': branch,
                'platform': target,
                'buildid': buildid,
            }
            sha = self.query_sha512sum(os.path.join(dirs['work_dir'], 'sources.xml'))
            xmlfilename = "sources-%s.xml" % sha
            tbpl_string = "TinderboxPrint: sources.xml: http://%s%s/%s" % (
                self.config['manifest']['upload_remote_host'],
                upload_remote_basepath,
                xmlfilename,
            )
            self.copy_to_upload_dir(
                os.path.join(dirs['work_dir'], 'sources.xml'),
                os.path.join(upload_dir, 'sources.xml')
            )

        self.copy_to_upload_dir(
            os.path.join(dirs['work_dir'], 'sources.xml'),
            os.path.join(upload_dir, xmlfilename)
        )
        retval = self.rsync_upload_directory(
            upload_dir,
            self.config['manifest']['ssh_key'],
            self.config['manifest']['ssh_user'],
            self.config['manifest']['upload_remote_host'],
            upload_remote_basepath,
        )
        if retval is not None:
            self.error("Failed to upload")
            self.return_code = 2
            return
        if tbpl_string:
            self.info(tbpl_string)

        if self.query_is_nightly():
            # run jgriffin's orgranize.py to shuffle the files around
            # https://github.com/jonallengriffin/b2gautomation/blob/master/b2gautomation/organize.py
            ssh = self.query_exe('ssh')
            cmd = [ssh,
                   '-l', self.config['manifest']['ssh_user'],
                   '-i', self.config['manifest']['ssh_key'],
                   self.config['manifest']['upload_remote_host'],
                   'python ~/organize.py --directory %s' % upload_remote_basepath,
                   ]
            retval = self.run_command(cmd)
            if retval != 0:
                self.error("Failed to move manifest to final location")
                self.return_code = 2
                return
        self.info("Upload successful")

    def submit_to_balrog(self):
        if not self.query_is_nightly():
            self.info("Not a nightly build, skipping balrog submission.")
            return

        if not self.config.get("balrog_servers"):
            self.info("balrog_servers not set; skipping balrog submission.")
            return

        self.checkout_tools()

        for update_type in self.update_types:
            marfile = self.query_marfile_path(update_type)
            # Need to update the base url to point at FTP, or better yet, read
            # post_upload.py output?
            mar_url = self.query_complete_mar_url(marfile)

            # Set other necessary properties for Balrog submission. None need to
            # be passed back to buildbot, so we won't write them to the properties
            # files.
            # Locale is hardcoded to en-US, for silly reasons
            self.set_buildbot_property("locale", "en-US")
            self.set_buildbot_property("appVersion", self.query_version())
            # The Balrog submitter translates this platform into a build target
            # via https://github.com/mozilla/build-tools/blob/master/lib/python/release/platforms.py#L23
            if "platform" in self.config:
                self.set_buildbot_property("platform", self.config["platform"])
            else:
                self.set_buildbot_property("platform", self.buildbot_config["properties"]["platform"])
            # TODO: Is there a better way to get this?
            self.set_buildbot_property("appName", "B2G")
            # TODO: don't hardcode
            self.set_buildbot_property("hashType", "sha512")
            self.set_buildbot_property("completeMarSize", self.query_filesize(marfile))
            self.set_buildbot_property("completeMarHash", self.query_sha512sum(marfile))
            self.set_buildbot_property("completeMarUrl", mar_url)
            self.set_buildbot_property("isOSUpdate", update_type.startswith("fota"))

            self.submit_balrog_updates(product='b2g')

    @PostScriptRun
    def _remove_userconfig(self):
        self.info("Cleanup .userconfig file.")
        dirs = self.query_abs_dirs()
        userconfig_path = os.path.join(dirs["work_dir"], ".userconfig")
        if os.path.exists(userconfig_path):
            os.remove(userconfig_path)

# main {{{1
if __name__ == '__main__':
    myScript = B2GBuild()
    myScript.run_and_exit()
