#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import BaseScript
from mozharness.mozilla.bouncer.submitter import BouncerSubmitterMixin
from mozharness.mozilla.buildbot import BuildbotMixin
from mozharness.mozilla.purge import PurgeMixin


class BouncerSubmitter(BaseScript, PurgeMixin, BouncerSubmitterMixin, BuildbotMixin):
    config_options = [
        [["--repo"], {
            "dest": "repo",
            "help": "Specify source repo, e.g. releases/mozilla-beta",
        }],
        [["--revision"], {
            "dest": "revision",
            "help": "Source revision/tag used to fetch shipped-locales",
        }],
        [["--version"], {
            "dest": "version",
            "help": "Current version",
        }],
        [["--previous-version"], {
            "dest": "prev_versions",
            "action": "extend",
            "help": "Previous version(s)",
        }],
        [["--build-number"], {
            "dest": "build_number",
            "help": "Build number of version",
        }],
        [["--bouncer-api-prefix"], {
            "dest": "bouncer-api-prefix",
            "help": "Bouncer admin API URL prefix",
        }],
        [["--credentials-file"], {
            "dest": "credentials_file",
            "help": "File containing Bouncer credentials",
        }],
    ]

    def __init__(self, require_config_file=True):
        BaseScript.__init__(self,
                            config_options=self.config_options,
                            require_config_file=require_config_file,
                            # other stuff
                            all_actions=[
                                'clobber',
                                'download-shipped-locales',
                                'submit',
                            ],
                            default_actions=[
                                'clobber',
                                'download-shipped-locales',
                                'submit',
                            ],
                            config={
                                 'buildbot_json_path' : 'buildprops.json'
                            }
                            )
        self.locales = None
        self.credentials = None

    def _pre_config_lock(self, rw_config):
        super(BouncerSubmitter, self)._pre_config_lock(rw_config)

        #override properties from buildbot properties here as defined by taskcluster properties
        self.read_buildbot_config()

        #check if release promotion is true first before overwriting these properties
        if self.buildbot_config["properties"].get("release_promotion"):
            for prop in ['product', 'version', 'build_number', 'revision', 'bouncer_submitter_config', ]:
                if self.buildbot_config["properties"].get(prop):
                    self.info("Overriding %s with %s" % (prop,  self.buildbot_config["properties"].get(prop)))
                    self.config[prop] = self.buildbot_config["properties"].get(prop)
            if self.buildbot_config["properties"].get("partial_versions"):
                self.config["prev_versions"] = self.buildbot_config["properties"].get("partial_versions").split(", ")

        for opt in ["version", "credentials_file", "bouncer-api-prefix"]:
            if opt not in self.config:
                self.fatal("%s must be specified" % opt)
        if self.need_shipped_locales():
            for opt in ["shipped-locales-url", "repo", "revision"]:
                if opt not in self.config:
                    self.fatal("%s must be specified" % opt)

    def need_shipped_locales(self):
        return any(e.get("add-locales") for e in
                   self.config["products"].values())

    def query_shipped_locales_path(self):
        dirs = self.query_abs_dirs()
        return os.path.join(dirs["abs_work_dir"], "shipped-locales")

    def download_shipped_locales(self):
        if not self.need_shipped_locales():
            self.info("No need to download shipped-locales")
            return

        replace_dict = {"revision": self.config["revision"],
                        "repo": self.config["repo"]}
        url = self.config["shipped-locales-url"] % replace_dict
        dirs = self.query_abs_dirs()
        self.mkdir_p(dirs["abs_work_dir"])
        if not self.download_file(url=url,
                                  file_name=self.query_shipped_locales_path()):
            self.fatal("Unable to fetch shipped-locales from %s" % url)
        # populate the list
        self.load_shipped_locales()

    def load_shipped_locales(self):
        if self.locales:
            return self.locales
        content = self.read_from_file(self.query_shipped_locales_path())
        locales = []
        for line in content.splitlines():
            locale = line.split()[0]
            if locale:
                locales.append(locale)
        self.locales = locales
        return self.locales

    def submit(self):
        subs = {
            "version": self.config["version"]
        }
        if self.config.get("build_number"):
            subs["build_number"] = self.config["build_number"]

        for product, pr_config in sorted(self.config["products"].items()):
            product_name = pr_config["product-name"] % subs
            if self.product_exists(product_name):
                self.warning("Product %s already exists. Skipping..." %
                             product_name)
                continue
            self.info("Adding %s..." % product)
            self.api_add_product(
                product_name=product_name,
                add_locales=pr_config.get("add-locales"),
                ssl_only=pr_config.get("ssl-only"))
            self.info("Adding paths...")
            for platform, pl_config in sorted(pr_config["paths"].items()):
                bouncer_platform = pl_config["bouncer-platform"]
                path = pl_config["path"] % subs
                self.info("%s (%s): %s" % (platform, bouncer_platform, path))
                self.api_add_location(product_name, bouncer_platform, path)

        # Add partial updates
        if "partials" in self.config and self.config.get("prev_versions"):
            self.submit_partials()

    def submit_partials(self):
        subs = {
            "version": self.config["version"]
        }
        if self.config.get("build_number"):
            subs["build_number"] = self.config["build_number"]
        prev_versions = self.config.get("prev_versions")
        for product, part_config in sorted(self.config["partials"].items()):
            product_name_tmpl = part_config["product-name"]
            for prev_version in prev_versions:
                prev_version, prev_build_number = prev_version.split("build")
                subs["prev_version"] = prev_version
                subs["prev_build_number"] = prev_build_number
                product_name = product_name_tmpl % subs
                if self.product_exists(product_name):
                    self.warning("Product %s already exists. Skipping..." %
                                 product_name)
                    continue
                self.info("Adding partial updates for %s" % product_name)
                self.api_add_product(
                    product_name=product_name,
                    add_locales=part_config.get("add-locales"),
                    ssl_only=part_config.get("ssl-only"))
                for platform, pl_config in sorted(part_config["paths"].items()):
                    bouncer_platform = pl_config["bouncer-platform"]
                    path = pl_config["path"] % subs
                    self.info("%s (%s): %s" % (platform, bouncer_platform, path))
                    self.api_add_location(product_name, bouncer_platform, path)


if __name__ == '__main__':
    myScript = BouncerSubmitter()
    myScript.run_and_exit()
