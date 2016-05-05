#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" uptake_monitoring.py

A script to replace the old-fashion way of computing the uptake monitoring
from the scheduler within the slaves.
"""

import os
import sys
import datetime
import time
import xml.dom.minidom

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.python import VirtualenvMixin, virtualenv_config_options
from mozharness.base.script import BaseScript
from mozharness.mozilla.buildbot import BuildbotMixin


def get_tuxedo_uptake_url(tuxedo_server_url, related_product, os):
    return '%s/uptake/?product=%s&os=%s' % (tuxedo_server_url,
                                            related_product, os)


class UptakeMonitoring(BaseScript, VirtualenvMixin, BuildbotMixin):
    config_options = virtualenv_config_options

    def __init__(self, require_config_file=True):
        super(UptakeMonitoring, self).__init__(
            config_options=self.config_options,
            require_config_file=require_config_file,
            config={
                "virtualenv_modules": [
                    "redo",
                    "requests",
                ],

                "virtualenv_path": "venv",
                "credentials_file": "oauth.txt",
                "buildbot_json_path": "buildprops.json",
                "poll_interval": 60,
                "poll_timeout": 20*60,
                "min_uptake": 10000,
            },
            all_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "monitor-uptake",
            ],
            default_actions=[
                "create-virtualenv",
                "activate-virtualenv",
                "monitor-uptake",
            ],
        )

    def _pre_config_lock(self, rw_config):
        super(UptakeMonitoring, self)._pre_config_lock(rw_config)
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
        partials = [v.strip() for v in props["partial_versions"].split(",")]
        self.config["partial_versions"] = [v.split("build")[0] for v in partials]
        self.config["platforms"] = [p.strip() for p in
                                    props["platforms"].split(",")]

    def _get_product_uptake(self, tuxedo_server_url, auth,
                            related_product, os):
        from redo import retry
        import requests

        url = get_tuxedo_uptake_url(tuxedo_server_url, related_product, os)
        self.info("Requesting {} from tuxedo".format(url))

        def get_tuxedo_page():
            r = requests.get(url, auth=auth,
                             verify=False, timeout=60)
            r.raise_for_status()
            return r.content

        def calculateUptake(page):
            doc = xml.dom.minidom.parseString(page)
            uptake_values = []

            for element in doc.getElementsByTagName('available'):
                for node in element.childNodes:
                    if node.nodeType == xml.dom.minidom.Node.TEXT_NODE and \
                            node.data.isdigit():
                        uptake_values.append(int(node.data))
            if not uptake_values:
                uptake_values = [0]
            return min(uptake_values)

        page = retry(get_tuxedo_page)
        uptake = calculateUptake(page)
        self.info("Current uptake for {} is {}".format(related_product, uptake))
        return uptake

    def _get_release_uptake(self, auth):
        assert isinstance(self.config["platforms"], (list, tuple))

        # handle the products first
        tuxedo_server_url = self.config["tuxedo_server_url"]
        version = self.config["version"]
        dl = []

        for product, info in self.config["products"].iteritems():
            if info.get("check_uptake"):
                product_template = info["product-name"]
                related_product = product_template % {"version": version}

                enUS_platforms = set(self.config["platforms"])
                paths_platforms = set(info["paths"].keys())
                platforms = enUS_platforms.intersection(paths_platforms)

                for platform in platforms:
                    bouncer_platform = info["paths"].get(platform).get('bouncer-platform')
                    dl.append(self._get_product_uptake(tuxedo_server_url, auth,
                                                       related_product, bouncer_platform))
        # handle the partials as well
        prev_versions = self.config["partial_versions"]
        for product, info in self.config["partials"].iteritems():
            if info.get("check_uptake"):
                product_template = info["product-name"]
                for prev_version in prev_versions:
                    subs = {
                        "version": version,
                        "prev_version": prev_version
                    }
                    related_product = product_template % subs

                    enUS_platforms = set(self.config["platforms"])
                    paths_platforms = set(info["paths"].keys())
                    platforms = enUS_platforms.intersection(paths_platforms)

                    for platform in platforms:
                        bouncer_platform = info["paths"].get(platform).get('bouncer-platform')
                        dl.append(self._get_product_uptake(tuxedo_server_url, auth,
                                                           related_product, bouncer_platform))
        return min(dl)

    def monitor_uptake(self):
        credentials_file = os.path.join(os.getcwd(),
                                        self.config["credentials_file"])
        credentials = {}
        execfile(credentials_file, credentials)
        auth = (credentials['tuxedoUsername'], credentials['tuxedoPassword'])
        self.info("Starting the loop to determine the uptake monitoring ...")

        start_time = datetime.datetime.now()
        while True:
            delta = (datetime.datetime.now() - start_time).seconds
            if delta > self.config["poll_timeout"]:
                self.error("Uptake monitoring sadly timed-out")
                raise Exception("Time-out during uptake monitoring")

            uptake = self._get_release_uptake(auth)
            self.info("Current uptake value to check is {}".format(uptake))

            if uptake >= self.config["min_uptake"]:
                self.info("Uptake monitoring is complete!")
                break
            else:
                self.info("Mirrors not yet updated, sleeping for a bit ...")
                time.sleep(self.config["poll_interval"])


if __name__ == '__main__':
    myScript = UptakeMonitoring()
    myScript.run_and_exit()
