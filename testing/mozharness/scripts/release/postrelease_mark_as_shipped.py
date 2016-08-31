#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" postrelease_mark_as_shipped.py

A script to automate the manual way of updating a release as shipped in Ship-it
following its successful ship-to-the-door opertion.
"""
import os
import sys
from datetime import datetime

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.mozilla.buildbot import BuildbotMixin


def build_release_name(product, version, buildno):
    """Function to reconstruct the name of the release based on product,
    version and buildnumber
    """
    return "{}-{}-build{}".format(product.capitalize(),
                                  str(version), str(buildno))


class MarkReleaseAsShipped(BaseScript, VirtualenvMixin, BuildbotMixin):
    config_options = virtualenv_config_options

    def __init__(self, require_config_file=True):
        super(MarkReleaseAsShipped, self).__init__(
            config_options=self.config_options,
            require_config_file=require_config_file,
            config={
                "virtualenv_modules": [
                    "shipitapi",
                ],
                "virtualenv_path": "venv",
                "credentials_file": "oauth.txt",
                "buildbot_json_path": "buildprops.json",
                "timeout": 60,
            },
            all_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "mark-as-shipped",
            ],
            default_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "mark-as-shipped",
            ],
        )

    def _pre_config_lock(self, rw_config):
        super(MarkReleaseAsShipped, self)._pre_config_lock(rw_config)
        # override properties from buildbot properties here as defined by
        # taskcluster properties
        self.read_buildbot_config()
        if not self.buildbot_config:
            self.warning("Skipping buildbot properties overrides")
            return
        props = self.buildbot_config['properties']
        mandatory_props = ['product', 'version', 'build_number']
        missing_props = []
        for prop in mandatory_props:
            if prop in props:
                self.info("Overriding %s with %s" % (prop, props[prop]))
                self.config[prop] = props.get(prop)
            else:
                self.warning("%s could not be found within buildprops" % prop)
                missing_props.append(prop)

        if missing_props:
            raise Exception("%s not found in configs" % missing_props)

        self.config['name'] = build_release_name(self.config['product'],
                                                 self.config['version'],
                                                 self.config['build_number'])

    def mark_as_shipped(self):
        """Method to make a simple call to Ship-it API to change a release
        status to 'shipped'
        """
        credentials_file = os.path.join(os.getcwd(),
                                        self.config["credentials_file"])
        credentials = {}
        execfile(credentials_file, credentials)
        ship_it_credentials = credentials["ship_it_credentials"]
        auth = (self.config["ship_it_username"],
                ship_it_credentials.get(self.config["ship_it_username"]))
        api_root = self.config['ship_it_root']

        from shipitapi import Release
        release_api = Release(auth, api_root=api_root,
                              timeout=self.config['timeout'])
        shipped_at = datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')

        self.info("Mark the release as shipped with %s timestamp" % shipped_at)
        release_api.update(self.config['name'],
                           status='shipped', shippedAt=shipped_at)


if __name__ == '__main__':
    MarkReleaseAsShipped().run_and_exit()
