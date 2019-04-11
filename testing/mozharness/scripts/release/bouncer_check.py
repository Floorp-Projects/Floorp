#!/usr/bin/env python
# lint_ignore=E501
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
""" bouncer_check.py

A script to check HTTP statuses of Bouncer products to be shipped.
"""

import os
import sys

sys.path.insert(1, os.path.dirname(os.path.dirname(sys.path[0])))

from mozharness.base.script import BaseScript
from mozharness.mozilla.automation import EXIT_STATUS_DICT, TBPL_FAILURE

BOUNCER_URL_PATTERN = "{bouncer_prefix}?product={product}&os={os}&lang={lang}"


class BouncerCheck(BaseScript):
    config_options = [
        [["--version"], {
            "dest": "version",
            "help": "Version of release, eg: 39.0b5",
        }],
        [["--product-field"], {
            "dest": "product_field",
            "help": "Version field of release from product details, eg: LATEST_FIREFOX_VERSION",
        }],
        [["--products-url"], {
            "dest": "products_url",
            "help": "The URL of the current Firefox product versions",
            "type": str,
            "default": "https://product-details.mozilla.org/1.0/firefox_versions.json",
        }],
        [["--previous-version"], {
            "dest": "prev_versions",
            "action": "extend",
            "help": "Previous version(s)",
        }],
        [["--locale"], {
            "dest": "locales",
            # Intentionally limited for several reasons:
            # 1) faster to check
            # 2) do not need to deal with situation when a new locale
            # introduced and we do not have partials for it yet
            # 3) it mimics the old Sentry behaviour that worked for ages
            # 4) no need to handle ja-JP-mac
            "default": ["en-US", "de", "it", "zh-TW"],
            "action": "append",
            "help": "List of locales to check.",
        }],
        [["-j", "--parallelization"], {
            "dest": "parallelization",
            "default": 20,
            "type": int,
            "help": "Number of HTTP sessions running in parallel",
        }],
    ]

    def __init__(self, require_config_file=True):
        super(BouncerCheck, self).__init__(
            config_options=self.config_options,
            require_config_file=require_config_file,
            config={
                "cdn_urls": [
                    'download-installer.cdn.mozilla.net',
                    'download.cdn.mozilla.net',
                    'download.mozilla.org',
                    'archive.mozilla.org',
                ],
            },
            all_actions=[
                "check-bouncer",
            ],
            default_actions=[
                "check-bouncer",
            ],
        )

    def _pre_config_lock(self, rw_config):
        super(BouncerCheck, self)._pre_config_lock(rw_config)

        if "product_field" not in self.config:
            return

        firefox_versions = self.load_json_url(self.config["products_url"])

        if self.config['product_field'] not in firefox_versions:
            self.fatal('Unknown Firefox label: {}'.format(self.config['product_field']))
        self.config["version"] = firefox_versions[self.config["product_field"]]
        self.log("Set Firefox version {}".format(self.config["version"]))

    def check_url(self, session, url):
        from redo import retry
        from requests.exceptions import HTTPError
        try:
            from urllib.parse import urlparse
        except ImportError:
            # Python 2
            from urlparse import urlparse

        def do_check_url():
            self.log("Checking {}".format(url))
            r = session.head(url, verify=True, timeout=10, allow_redirects=True)
            try:
                r.raise_for_status()
            except HTTPError:
                self.error("FAIL: {}, status: {}".format(url, r.status_code))
                raise

            final_url = urlparse(r.url)
            if final_url.scheme != 'https':
                self.error('FAIL: URL scheme is not https: {}'.format(r.url))
                self.return_code = EXIT_STATUS_DICT[TBPL_FAILURE]

            if final_url.netloc not in self.config['cdn_urls']:
                self.error('FAIL: host not in allowed locations: {}'.format(r.url))
                self.return_code = EXIT_STATUS_DICT[TBPL_FAILURE]

        try:
            retry(do_check_url, sleeptime=3, max_sleeptime=10, attempts=3)
        except HTTPError:
            # The error was already logged above.
            self.return_code = EXIT_STATUS_DICT[TBPL_FAILURE]
            return

    def get_urls(self):
        for product in self.config["products"].values():
            if not product["check_uptake"]:
                continue
            product_name = product["product-name"] % {"version": self.config["version"]}
            for bouncer_platform in product["platforms"]:
                for locale in self.config["locales"]:
                    url = BOUNCER_URL_PATTERN.format(
                        bouncer_prefix=self.config["bouncer_prefix"],
                        product=product_name,
                        os=bouncer_platform,
                        lang=locale,
                    )
                    yield url

        for product in self.config.get("partials", {}).values():
            if not product["check_uptake"]:
                continue
            for prev_version in self.config.get("prev_versions", []):
                product_name = product["product-name"] % {"version": self.config["version"],
                                                          "prev_version": prev_version}
                for bouner_platform in product["platforms"]:
                    for locale in self.config["locales"]:
                        url = BOUNCER_URL_PATTERN.format(
                            bouncer_prefix=self.config["bouncer_prefix"],
                            product=product_name,
                            os=bouncer_platform,
                            lang=locale,
                        )
                        yield url

    def check_bouncer(self):
        import requests
        import concurrent.futures as futures
        session = requests.Session()
        http_adapter = requests.adapters.HTTPAdapter(
            pool_connections=self.config["parallelization"],
            pool_maxsize=self.config["parallelization"])
        session.mount('https://', http_adapter)
        session.mount('http://', http_adapter)

        with futures.ThreadPoolExecutor(self.config["parallelization"]) as e:
            fs = []
            for url in self.get_urls():
                fs.append(e.submit(self.check_url, session, url))
            for f in futures.as_completed(fs):
                f.result()


if __name__ == '__main__':
    BouncerCheck().run_and_exit()
