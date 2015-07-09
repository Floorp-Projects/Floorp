#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""firefox_ui_updates.py

Author: Armen Zambrano G.
"""
import copy
import os
import urllib
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.log import INFO
from mozharness.base.script import PreScriptAction
from mozharness.mozilla.testing.firefox_ui_tests import FirefoxUITests


class FirefoxUIUpdates(FirefoxUITests):
    # This will be a list containing one item per release based on configs
    # from tools/release/updates/*cfg
    releases = None
    channel = None
    harness_extra_args = [
        [['--update-allow-mar-channel'], {
            'dest': 'update_allow_mar_channel',
            'help': 'Additional MAR channel to be allowed for updates, e.g. '
                    '"firefox-mozilla-beta" for updating a release build to '
                    'the latest beta build.',
        }],
        [['--update-channel'], {
            'dest': 'update_channel',
            'help': 'Update channel to use.',
        }],
        [['--update-target-version'], {
            'dest': 'update_target_version',
            'help': 'Version of the updated build.',
        }],
        [['--update-target-buildid'], {
            'dest': 'update_target_buildid',
            'help': 'Build ID of the updated build',
        }],
    ]


    def __init__(self):
        config_options = [
            [['--tools-repo'], {
                'dest': 'tools_repo',
                'default': 'http://hg.mozilla.org/build/tools',
                'help': 'Which tools repo to check out',
            }],
            [['--tools-tag'], {
                'dest': 'tools_tag',
                'help': 'Which revision/tag to use for the tools repository.',
            }],
            [['--update-verify-config'], {
                'dest': 'update_verify_config',
                'help': 'Which update verify config file to use.',
            }],
            [['--this-chunk'], {
                'dest': 'this_chunk',
                'default': 1,
                'help': 'What chunk of locales to process.',
            }],
            [['--total-chunks'], {
                'dest': 'total_chunks',
                'default': 1,
                'help': 'Total chunks to dive the locales into.',
            }],
            [['--dry-run'], {
                'dest': 'dry_run',
                'default': False,
                'help': 'Only show what was going to be tested.',
            }],
            # These are options when we don't use the releng update config file
            [['--installer-url'], {
                'dest': 'installer_url',
                'help': 'Point to an installer to download and test against.',
            }],
            [['--installer-path'], {
                'dest': 'installer_path',
                'help': 'Point to an installer to test against.',
            }],
        ] + copy.deepcopy(self.harness_extra_args)

        super(FirefoxUIUpdates, self).__init__(
            config_options=config_options,
            all_actions=[
                'purge-builds',
                'clobber',
                'checkout',
                'create-virtualenv',
                'determine-testing-configuration',
                'run-tests',
            ],
        )

        dirs = self.query_abs_dirs()

        if self.config.get('tools_tag') is None:
            # We want to make sure that anyone trying to reproduce a job will
            # is using the exact tools tag for reproducibility's sake
            self.fatal('Make sure to specify the --tools-tag')

        self.tools_repo = self.config['tools_repo']
        self.tools_tag = self.config['tools_tag']

        if self.config.get('update_verify_config'):
            self.updates_config_file = os.path.join(
                dirs['tools_dir'], 'release', 'updates',
                self.config['update_verify_config']
            )
        else:
            self.fatal('Make sure to specify --update-verify-config. '
                       'See under the directory release/updates in %s.' % self.tools_repo)

        self.installer_url = self.config.get('installer_url')
        self.installer_path = self.config.get('installer_path')

        if self.installer_path:
            if not os.path.exists(self.installer_path):
                self.critical('Please make sure that the path to the installer exists.')
                exit(1)

        assert 'update_verify_config' in self.config or self.installer_url or self.installer_path, \
            'Either specify --update-verify-config, --installer-url or --installer-path.'


    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxUIUpdates, self).query_abs_dirs()

        dirs = {
            'tools_dir': os.path.join(abs_dirs['abs_work_dir'], 'tools'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def checkout(self):
        '''
        This checkouts the tools repo because it contains the configuration
        files about which locales to test.

        We also checkout firefox_ui_tests and update to the right branch
        for it.
        '''
        super(FirefoxUIUpdates, self).checkout()
        dirs = self.query_abs_dirs()

        self.vcs_checkout(
            repo=self.tools_repo,
            dest=dirs['tools_dir'],
            revision=self.tools_tag,
            vcs='hgtool'
        )


    def determine_testing_configuration(self):
        '''
        This method builds a testing matrix either based on an update verification
        configuration file under the tools repo (release/updates/*.cfg)
        OR it skips it when we use --installer-url --installer-path

        Each release info line of the update verification files look similar to the following.

        NOTE: This shows each pair of information as a new line but in reality
        there is one white space separting them. We only show the values we care for.

            release="38.0"
            platform="Linux_x86_64-gcc3"
            build_id="20150429135941"
            locales="ach af ... zh-TW"
            channel="beta-localtest"
            from="/firefox/releases/38.0b9/linux-x86_64/%locale%/firefox-38.0b9.tar.bz2"
            ftp_server_from="http://stage.mozilla.org/pub/mozilla.org"

        We will store this information in self.releases as a list of releases.

        NOTE: We will talk of full and quick releases. Full release info normally contains a subset
        of all locales (except for the most recent releases). A quick release has all locales,
        however, it misses the fields 'from' and 'ftp_server_from'.
        Both pairs of information complement each other but differ in such manner.
        '''
        if self.installer_url or self.installer_path:
            return

        dirs = self.query_abs_dirs()
        assert os.path.exists(dirs['tools_dir']), \
            "Without the tools/ checkout we can't use releng's config parser."

        # Import the config parser
        sys.path.insert(1, os.path.join(dirs['tools_dir'], 'lib', 'python'))
        from release.updates.verify import UpdateVerifyConfig

        uvc = UpdateVerifyConfig()
        uvc.read(self.updates_config_file)
        self.channel = uvc.channel

        # Filter out any releases that are less than Gecko 38
        uvc.releases = [r for r in uvc.releases \
                if int(r["release"].split('.')[0]) >= 38]

        temp_releases = []
        for rel_info in uvc.releases:
            # This is the full release info
            if 'from' in rel_info and rel_info['from'] is not None:
                # Let's find the associated quick release which contains the remaining locales
                # for all releases except for the most recent release which contain all locales
                quick_release = uvc.getRelease(build_id=rel_info['build_id'], from_path=None)
                if quick_release != {}:
                    rel_info['locales'] = sorted(rel_info['locales'] + quick_release['locales'])
                temp_releases.append(rel_info)

        uvc.releases = temp_releases
        chunked_config = uvc.getChunk(
            chunks=int(self.config['total_chunks']),
            thisChunk=int(self.config['this_chunk'])
        )

        self.releases = chunked_config.releases


    @PreScriptAction('run-tests')
    def _pre_run_tests(self, action):
        if self.releases is None and not (self.installer_url or self.installer_path):
            self.critical('You need to call --determine-testing-configuration as well.')
            exit(1)


    def _run_test(self, installer_path, update_channel=None, cleanup=True,
                  marionette_port=2828):
        '''
        All required steps for running the tests against an installer.
        '''
        env = self.query_env()
        dirs = self.query_abs_dirs()
        bin_dir = os.path.dirname(self.query_python_path())
        fx_ui_tests_bin = os.path.join(bin_dir, 'firefox-ui-update')
        gecko_log=os.path.join(dirs['abs_work_dir'], 'gecko.log')

        # Build the command
        cmd = [
            fx_ui_tests_bin,
            '--installer', installer_path,
            # Log to stdout until tests are stable.
            '--gecko-log=-',
            '--address=localhost:%s' % marionette_port,
        ]

        for arg in self.harness_extra_args:
            dest = arg[1]['dest']
            if dest in self.config:
                cmd += [' '.join(arg[0]), self.config[dest]]

        if update_channel:
            cmd += ['--update-channel', update_channel]

        return_code = self.run_command(cmd, cwd=dirs['abs_work_dir'],
                                       output_timeout=300,
                                       env=env)

        # Return more output if we fail
        if return_code != 0:
            if os.path.exists(gecko_log):
                contents = self.read_from_file(gecko_log, verbose=False)
                self.warning('== Dumping gecko output ==')
                self.warning(contents)
                self.warning('== End of gecko output ==')
            else:
                self.warning('No gecko.log was found: %s' % gecko_log)

        if cleanup:
            for filepath in (installer_path, gecko_log):
                if os.path.exists(filepath):
                    self.debug('Removing %s' % filepath)
                    os.remove(filepath)

        return return_code


    def run_tests(self):
        dirs = self.query_abs_dirs()

        if self.installer_url or self.installer_path:
            if self.installer_url:
                self.installer_path = self.download_file(
                    self.installer_url,
                    parent_dir=dirs['abs_work_dir']
                )

            return self._run_test(self.installer_path, cleanup=False)

        else:
            for rel_info in sorted(self.releases, key=lambda release: release['build_id']):
                self.info('About to run %s %s - %s locales' % (
                    rel_info['build_id'],
                    rel_info['from'],
                    len(rel_info['locales'])
                ))

                if self.config['dry_run']:
                    continue

                # Each locale gets a fresh port to avoid address in use errors in case of
                # tests that time out unexpectedly.
                marionette_port = 2827
                for locale in rel_info['locales']:
                    # Determine from where to download the file
                    url = '%s/%s' % (
                        rel_info['ftp_server_from'],
                        urllib.quote(rel_info['from'].replace('%locale%', locale))
                    )
                    installer_path = self.download_file(
                        url=url,
                        parent_dir=dirs['abs_work_dir']
                    )

                    marionette_port += 1
                    retcode = self._run_test(installer_path, self.channel,
                                             marionette_port=marionette_port)
                    if retcode != 0:
                        self.warning('FAIL: firefox-ui-update has failed.' )
                        self.info('You can run the following command on the same machine to reproduce the issue:')
                        self.info('python scripts/firefox_ui_updates.py --cfg generic_releng_config.py '
                                  '--firefox-ui-branch %s --update-verify-config %s '
                                  '--tools-tag %s --installer-url %s '
                                  '--determine-testing-configuration --run-tests '
                                  % (self.firefox_ui_branch, self.updates_config_file, self.tools_tag, url))
                        self.info('If you want to run this on your development machine:')
                        self.info('python scripts/firefox_ui_updates.py '
                                  '--firefox-ui-branch %s --update-verify-config %s '
                                  '--tools-tag %s --installer-url %s '
                                  '--cfg developer_config.py '
                                  % (self.firefox_ui_branch, self.updates_config_file, self.tools_tag, url))


if __name__ == '__main__':
    myScript = FirefoxUIUpdates()
    myScript.run_and_exit()
