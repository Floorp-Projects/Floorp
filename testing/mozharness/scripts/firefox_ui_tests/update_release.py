#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****


import copy
import os
import pprint
import sys
import urllib

# load modules from parent dir
sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import PreScriptAction
from mozharness.mozilla.automation import TBPL_SUCCESS, TBPL_WARNING, EXIT_STATUS_DICT
from mozharness.mozilla.testing.firefox_ui_tests import (
    FirefoxUIUpdateTests,
    firefox_ui_update_config_options
)


# Command line arguments for release update tests
firefox_ui_update_release_config_options = [
    [['--build-number'], {
        'dest': 'build_number',
        'help': 'Build number of release, eg: 2',
    }],
    [['--limit-locales'], {
        'dest': 'limit_locales',
        'default': -1,
        'type': int,
        'help': 'Limit the number of locales to run.',
    }],
    [['--release-update-config'], {
        'dest': 'release_update_config',
        'help': 'Name of the release update verification config file to use.',
    }],
    [['--this-chunk'], {
        'dest': 'this_chunk',
        'default': 1,
        'help': 'What chunk of locales to process.',
    }],
    [['--tools-repo'], {
        'dest': 'tools_repo',
        'default': 'http://hg.mozilla.org/build/tools',
        'help': 'Which tools repo to check out',
    }],
    [['--tools-tag'], {
        'dest': 'tools_tag',
        'help': 'Which revision/tag to use for the tools repository.',
    }],
    [['--total-chunks'], {
        'dest': 'total_chunks',
        'default': 1,
        'help': 'Total chunks to dive the locales into.',
    }],
] + copy.deepcopy(firefox_ui_update_config_options)


class ReleaseFirefoxUIUpdateTests(FirefoxUIUpdateTests):

    def __init__(self):
        all_actions = [
            'clobber',
            'checkout',
            'create-virtualenv',
            'query_minidump_stackwalk',
            'read-release-update-config',
            'run-tests',
        ]

        super(ReleaseFirefoxUIUpdateTests, self).__init__(
            all_actions=all_actions,
            default_actions=all_actions,
            config_options=firefox_ui_update_release_config_options,
            append_env_variables_from_configs=True,
        )

        self.tools_repo = self.config.get('tools_repo')
        self.tools_tag = self.config.get('tools_tag')

        assert self.tools_repo and self.tools_tag, \
            'Without the "--tools-tag" we can\'t clone the releng\'s tools repository.'

        self.limit_locales = int(self.config.get('limit_locales'))

        # This will be a list containing one item per release based on configs
        # from tools/release/updates/*cfg
        self.releases = None

    def checkout(self):
        """
        We checkout the tools repository and update to the right branch
        for it.
        """
        dirs = self.query_abs_dirs()

        super(ReleaseFirefoxUIUpdateTests, self).checkout()

        self.vcs_checkout(
            repo=self.tools_repo,
            dest=dirs['abs_tools_dir'],
            branch=self.tools_tag,
            vcs='hg'
        )

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs

        abs_dirs = super(ReleaseFirefoxUIUpdateTests, self).query_abs_dirs()
        dirs = {
            'abs_tools_dir': os.path.join(abs_dirs['abs_work_dir'], 'tools'),
        }

        for key in dirs:
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs

        return self.abs_dirs

    def read_release_update_config(self):
        '''
        Builds a testing matrix based on an update verification configuration
        file under the tools repository (release/updates/*.cfg).

        Each release info line of the update verification files look similar to the following.

        NOTE: This shows each pair of information as a new line but in reality
        there is one white space separting them. We only show the values we care for.

            release="38.0"
            platform="Linux_x86_64-gcc3"
            build_id="20150429135941"
            locales="ach af ... zh-TW"
            channel="beta-localtest"
            from="/firefox/releases/38.0b9/linux-x86_64/%locale%/firefox-38.0b9.tar.bz2"
            ftp_server_from="http://archive.mozilla.org/pub"

        We will store this information in self.releases as a list of releases.

        NOTE: We will talk of full and quick releases. Full release info normally contains a subset
        of all locales (except for the most recent releases). A quick release has all locales,
        however, it misses the fields 'from' and 'ftp_server_from'.
        Both pairs of information complement each other but differ in such manner.
        '''
        dirs = self.query_abs_dirs()
        assert os.path.exists(dirs['abs_tools_dir']), \
            'Without the tools/ checkout we can\'t use releng\'s config parser.'

        if self.config.get('release_update_config'):
            # The config file is part of the tools repository. Make sure that if specified
            # we force a revision of that repository to be set.
            if self.tools_tag is None:
                self.fatal('Make sure to specify the --tools-tag')

            self.release_update_config = self.config['release_update_config']

        # Import the config parser
        sys.path.insert(1, os.path.join(dirs['abs_tools_dir'], 'lib', 'python'))
        from release.updates.verify import UpdateVerifyConfig

        uvc = UpdateVerifyConfig()
        config_file = os.path.join(dirs['abs_tools_dir'], 'release', 'updates',
                                   self.config['release_update_config'])
        uvc.read(config_file)
        if not hasattr(self, 'update_channel'):
            self.update_channel = uvc.channel

        # Filter out any releases that are less than Gecko 38
        uvc.releases = [r for r in uvc.releases
                        if int(r['release'].split('.')[0]) >= 38]

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
        assert ('release_update_config' in self.config or
                self.installer_url or self.installer_path), \
            'Either specify --update-verify-config, --installer-url or --installer-path.'

    def run_tests(self):
        dirs = self.query_abs_dirs()

        # We don't want multiple outputs of the same environment information. To prevent
        # that, we can't make it an argument of run_command and have to print it on our own.
        self.info('Using env: {}'.format(pprint.pformat(self.query_env())))

        results = {}

        locales_counter = 0
        for rel_info in sorted(self.releases, key=lambda release: release['build_id']):
            build_id = rel_info['build_id']
            results[build_id] = {}

            self.info('About to run {buildid} {path} - {num_locales} locales'.format(
                buildid=build_id,
                path=rel_info['from'],
                num_locales=len(rel_info['locales'])
            ))

            # Each locale gets a fresh port to avoid address in use errors in case of
            # tests that time out unexpectedly.
            marionette_port = 2827
            for locale in rel_info['locales']:
                locales_counter += 1
                self.info('Running {buildid} {locale}'.format(buildid=build_id,
                                                              locale=locale))

                if self.limit_locales > -1 and locales_counter > self.limit_locales:
                    self.info('We have reached the limit of locales we were intending to run')
                    break

                if self.config['dry_run']:
                    continue

                # Determine from where to download the file
                installer_url = '{server}/{fragment}'.format(
                    server=rel_info['ftp_server_from'],
                    fragment=urllib.quote(rel_info['from'].replace('%locale%', locale))
                )
                installer_path = self.download_file(
                    url=installer_url,
                    parent_dir=dirs['abs_work_dir']
                )

                binary_path = self.install_app(app=self.config.get('application'),
                                               installer_path=installer_path)

                marionette_port += 1

                retcode = self.run_test(
                    binary_path=binary_path,
                    env=self.query_env(avoid_host_env=True),
                    marionette_port=marionette_port,
                )

                self.uninstall_app()

                # Remove installer which is not needed anymore
                self.info('Removing {}'.format(installer_path))
                os.remove(installer_path)

                if retcode:
                    self.warning('FAIL: {} has failed.'.format(sys.argv[0]))

                    base_cmd = 'python {command} --firefox-ui-branch {branch} ' \
                        '--release-update-config {config} --tools-tag {tag}'.format(
                            command=sys.argv[0],
                            branch=self.firefox_ui_branch,
                            config=self.release_update_config,
                            tag=self.tools_tag
                        )

                    for config in self.config['config_files']:
                        base_cmd += ' --cfg {}'.format(config)

                    if self.symbols_url:
                        base_cmd += ' --symbols-path {}'.format(self.symbols_url)

                    base_cmd += ' --installer-url {}'.format(installer_url)

                    self.info('You can run the *specific* locale on the same machine with:')
                    self.info(base_cmd)

                    self.info('You can run the *specific* locale on *your* machine with:')
                    self.info('{} --cfg developer_config.py'.format(base_cmd))

                results[build_id][locale] = retcode

                self.info('Completed {buildid} {locale} with return code: {retcode}'.format(
                    buildid=build_id,
                    locale=locale,
                    retcode=retcode))

            if self.limit_locales > -1 and locales_counter > self.limit_locales:
                break

        # Determine which locales have failed and set scripts exit code
        exit_status = TBPL_SUCCESS
        for build_id in sorted(results.keys()):
            failed_locales = []
            for locale in sorted(results[build_id].keys()):
                if results[build_id][locale] != 0:
                    failed_locales.append(locale)

            if failed_locales:
                if exit_status == TBPL_SUCCESS:
                    self.info('\nSUMMARY - Failed locales for {}:'.format(self.cli_script))
                    self.info('====================================================')
                    exit_status = TBPL_WARNING

                self.info(build_id)
                self.info('  {}'.format(', '.join(failed_locales)))

        self.return_code = EXIT_STATUS_DICT[exit_status]


if __name__ == '__main__':
    myScript = ReleaseFirefoxUIUpdateTests()
    myScript.run_and_exit()
