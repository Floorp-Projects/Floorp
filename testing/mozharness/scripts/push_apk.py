#!/usr/bin/env python
""" push_apk.py

    Upload the apk of a Firefox app on Google play
    Example for a beta upload:
    $ python push_apk.py --package-name org.mozilla.firefox_beta --service-account foo@developer.gserviceaccount.com --credentials key.p12 --apk-x86=/path/to/fennec-XX.0bY.multi.android-i386.apk --apk-armv7-v9=/path/to/fennec-XX.0bY.multi.android-arm-v9.apk --apk-armv7-v15=/path/to/fennec-XX.0bY.multi.android-arm-v15.apk --track production --push_apk
"""
import sys
import os

from oauth2client import client

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

# import the guts
from mozharness.base.script import BaseScript
from mozharness.mozilla.googleplay import GooglePlayMixin
from mozharness.base.python import VirtualenvMixin


class PushAPK(BaseScript, GooglePlayMixin, VirtualenvMixin):
    all_actions = [
        'create-virtualenv',
        'push_apk',
        'test',
    ]

    default_actions = [
        'create-virtualenv',
        'test',
    ]
    config_options = [
        [["--track"], {
            "dest": "track",
            "help": "Track on which to upload "
            "(production, beta, alpha, rollout)",
            # We are not using alpha but we default to it to avoid mistake
            "default": "alpha"
        }],
        [["--service-account"], {
            "dest": "service_account",
            "help": "The service account email",
        }],
        [["--credentials"], {
            "dest": "google_play_credentials_file",
            "help": "The p12 authentication file",
            "default": "key.p12"
        }],
        [["--package-name"], {
            "dest": "package_name",
            "help": "The Google play name of the app",
        }],
        [["--apk-x86"], {
            "dest": "apk_file_x86",
            "help": "The path to the x86 APK file",
        }],
        [["--apk-armv7-v9"], {
            "dest": "apk_file_armv7_v9",
            "help": "The path to the ARM v7 API v9 APK file",
        }],
        [["--apk-armv7-v15"], {
            "dest": "apk_file_armv7_v15",
            "help": "The path to the ARM v7 API v15 APK file",
        }],
        [["--apk-armv6"], {
            "dest": "apk_file_armv6",
            "help": "The path to the ARM v6 APK file",
        }],


    ]

    # Google play has currently 3 tracks. Rollout deploys
    # to a limited percentage of users
    track_values = ("production", "beta", "alpha", "rollout")

    # We have 3 apps. Make sure that their names are correct
    package_name_values = ("org.mozilla.fennec_aurora",
                           "org.mozilla.firefox_beta",
                           "org.mozilla.firefox")

    def __init__(self, require_config_file=False, config={},
                 all_actions=all_actions,
                 default_actions=default_actions):

        # Default configuration
        default_config = {
            'debug_build': False,
            'pip_index': True,
            # this will pip install it automajically when we call the create-virtualenv action
            'virtualenv_modules': ['google-api-python-client'],
            "find_links": [   # so mozharness knows where to look for the package
                "http://pypi.pvt.build.mozilla.org/pub",
                "http://pypi.pub.build.mozilla.org/pub",
            ],
            # the path inside the work_dir ('build') of where we will install the env.
            # pretty sure it's the default and not needed.
            'virtualenv_path': 'venv',
        }
        default_config.update(config)

        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            config=default_config,
            all_actions=all_actions,
            default_actions=default_actions,
        )

    def check_argument(self):
        """ Check that the given values are correct,
        files exists, etc
        """
        if self.config['track'] not in self.track_values:
            self.fatal("Unknown track value " + self.config['track'])

        if self.config['package_name'] not in self.package_name_values:
            self.fatal("Unknown package name value " +
                       self.config['package_name'])

        if not os.path.isfile(self.config['apk_file_x86']):
            self.fatal("Could not find " + self.config['apk_file_x86'])

        if not os.path.isfile(self.config['apk_file_armv7_v9']):
            self.fatal("Could not find " + self.config['apk_file_armv7_v9'])

        if not os.path.isfile(self.config['apk_file_armv7_v15']):
            self.fatal("Could not find " + self.config['apk_file_armv7_v15'])

        if self.config.get('apk_file_armv6') and not os.path.isfile(self.config['apk_file_armv6']):
            self.fatal("Could not find " + self.config['apk_file_armv6'])

        if not os.path.isfile(self.config['google_play_credentials_file']):
            self.fatal("Could not find " + self.config['google_play_credentials_file'])

    def upload_apks(self, service, apk_files):
        """ Upload the APK to google play

        service -- The session to Google play
        apk_files -- The files
        """
        edit_request = service.edits().insert(body={},
                                              packageName=self.config['package_name'])
        result = edit_request.execute()
        edit_id = result['id']
        # Store all the versions to set the tracks (needs to happen
        # at the same time
        versions = []

        # For each files, upload it
        for apk_file in apk_files:
            try:
                # Upload the file
                apk_response = service.edits().apks().upload(
                    editId=edit_id,
                    packageName=self.config['package_name'],
                    media_body=apk_file).execute()
                self.log('Version code %d has been uploaded. '
                         'Filename "%s" edit_id %s' %
                         (apk_response['versionCode'], apk_file, edit_id))

                versions.append(apk_response['versionCode'])

            except client.AccessTokenRefreshError:
                self.log('The credentials have been revoked or expired,'
                         'please re-run the application to re-authorize')

        # Set the track for all apk
        service.edits().tracks().update(
            editId=edit_id,
            track=self.config['track'],
            packageName=self.config['package_name'],
            body={u'versionCodes': versions}).execute()
        self.log('Application "%s" set to track "%s" for versions %s' %
                 (self.config['package_name'], self.config['track'], versions))

        # Commit our changes
        commit_request = service.edits().commit(
            editId=edit_id, packageName=self.config['package_name']).execute()
        self.log('Edit "%s" has been committed' % (commit_request['id']))

    def push_apk(self):
        """ Upload the APK files """
        self.check_argument()
        service = self.connect_to_play()
        apks = [self.config['apk_file_armv7_v9'], self.config['apk_file_armv7_v15'], self.config['apk_file_x86']]
        if self.config.get('apk_file_armv6'):
            apks.append(self.config['apk_file_armv6'])
        self.upload_apks(service, apks)

    def test(self):
        """ Test if the connexion can be done """
        self.check_argument()
        self.connect_to_play()

# main {{{1
if __name__ == '__main__':
    myScript = PushAPK()
    myScript.run_and_exit()
