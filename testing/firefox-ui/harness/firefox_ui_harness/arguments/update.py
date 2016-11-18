# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from base import FirefoxUIArguments


class UpdateBaseArguments(object):
    name = 'Firefox UI Update Tests'
    args = [
        [['--update-allow-mar-channel'], {
            'dest': 'update_mar_channels',
            'default': [],
            'action': 'append',
            'metavar': 'MAR_CHANNEL',
            'help': 'Additional MAR channel to be allowed for updates, '
                    'e.g. "firefox-mozilla-beta" for updating a release '
                    'build to the latest beta build.'
        }],
        [['--update-channel'], {
            'metavar': 'CHANNEL',
            'help': 'Channel to use for the update check.'
        }],
        [['--update-direct-only'], {
            'default': False,
            'action': 'store_true',
            'help': 'Only perform a direct update'
        }],
        [['--update-fallback-only'], {
            'default': False,
            'action': 'store_true',
            'help': 'Only perform a fallback update'
        }],
        [['--update-url'], {
            'metavar': 'URL',
            'help': 'Force specified URL to use for update checks.'
        }],
        [['--update-target-version'], {
            'metavar': 'VERSION',
            'help': 'Version of the updated build.'
        }],
        [['--update-target-buildid'], {
            'metavar': 'BUILD_ID',
            'help': 'Build ID of the updated build.'
        }],
    ]

    def verify_usage_handler(self, args):
        if args.update_direct_only and args.update_fallback_only:
            raise ValueError('Arguments --update-direct-only and --update-fallback-only '
                             'are mutually exclusive.')


class UpdateArguments(FirefoxUIArguments):

    def __init__(self, **kwargs):
        super(UpdateArguments, self).__init__(**kwargs)

        self.register_argument_container(UpdateBaseArguments())
