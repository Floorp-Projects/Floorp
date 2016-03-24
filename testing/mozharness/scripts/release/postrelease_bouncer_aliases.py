#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" postrelease_bouncer_aliases.py

A script to replace the old-fashion way of updating the bouncer aliaes through
tools script.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.mozilla.buildbot import BuildbotMixin


# PostReleaseBouncerAliases {{{1
class PostReleaseBouncerAliases(BaseScript, VirtualenvMixin, BuildbotMixin):
    config_options = virtualenv_config_options

    def __init__(self, require_config_file=True):
        super(PostReleaseBouncerAliases, self).__init__(
            config_options=self.config_options,
            require_config_file=require_config_file,
            config={
                "virtualenv_modules": [
                    "redo",
                    "requests",
                ],
                "virtualenv_path": "venv",
                'credentials_file': 'oauth.txt',
                'buildbot_json_path': 'buildprops.json',
            },
            all_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "update-bouncer-aliases",
            ],
            default_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "update-bouncer-aliases",
            ],
        )

    def _pre_config_lock(self, rw_config):
        super(PostReleaseBouncerAliases, self)._pre_config_lock(rw_config)
        # override properties from buildbot properties here as defined by
        # taskcluster properties
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
            return
        props = self.buildbot_config["properties"]
        for prop in ['tuxedo_server_url', 'version']:
            if props.get(prop):
                self.info("Overriding %s with %s" % (prop, props[prop]))
                self.config[prop] = props.get(prop)
            else:
                self.warning("%s could not be found within buildprops" % prop)
                return

    def _update_bouncer_alias(self, tuxedo_server_url, auth,
                              related_product, alias):
        from redo import retry
        import requests

        url = "%s/create_update_alias" % tuxedo_server_url
        data = {"alias": alias, "related_product": related_product}
        self.log("Updating {} to point to {} using {}".format(alias,
                                                              related_product,
                                                              url))

        # Wrap the real call to hide credentials from retry's logging
        def do_update_bouncer_alias():
            r = requests.post(url, data=data, auth=auth,
                              verify=False, timeout=60)
            r.raise_for_status()

        retry(do_update_bouncer_alias)

    def update_bouncer_aliases(self):
        tuxedo_server_url = self.config['tuxedo_server_url']
        credentials_file = os.path.join(os.getcwd(),
                                        self.config['credentials_file'])
        credentials = {}
        execfile(credentials_file, credentials)
        auth = (credentials['tuxedoUsername'], credentials['tuxedoPassword'])
        version = self.config['version']
        for product, info in self.config["products"].iteritems():
            if "alias" in info:
                product_template = info["product-name"]
                related_product = product_template % {"version": version}
                self._update_bouncer_alias(tuxedo_server_url, auth,
                                           related_product, info["alias"])


# __main__ {{{1
if __name__ == '__main__':
    PostReleaseBouncerAliases().run_and_exit()
