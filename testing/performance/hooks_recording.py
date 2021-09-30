# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import platform
from pathlib import Path

from mozperftest.test.browsertime import add_options, add_option

options = [
    ("pageCompleteWaitTime", "10000"),
]

next_site = None

RECORDING_LIST = Path(Path(__file__).parent, "pageload_sites.json")


def before_iterations(kw):

    global next_site
    print("Setting up next site to record.")

    with RECORDING_LIST.open() as f:
        site_list = json.load(f)
    # currently can't record websites that require user interactions(Logins)
    if kw.get("android"):
        site_list = site_list["mobile"]
    else:
        site_list = site_list["desktop"]

    sites = [test_site for test_site in site_list if not test_site.get("login")]

    def next_site():
        for site in sites:
            yield site

    next_site = next_site()

    # Set the number of test-iterations to the number of builds
    kw["test_iterations"] = len(sites)
    return kw


def before_runs(env):
    global next_site
    print("Running before_runs")
    add_options(env, options)

    if next_site:
        test_site = next(next_site)
        print("Next site: %s" % test_site)

        if env.get_arg("android"):
            platform_name = "android"
            app_name = env.get_arg("android-app-name").split(".")[-1]
        else:
            platform_name = platform.system().lower()
            app_name = "firefox"

        name = [
            "mitm6",
            platform_name,
            "gve" if app_name == "geckoview_example" else app_name,
            test_site["name"],
        ]

        recording_file = "%s.zip" % "-".join(name)

        env.set_arg("proxy-mode", "record")
        env.set_arg(
            "proxy-file",
            recording_file,
        )

        add_option(env, "browsertime.url", test_site.get("test_url"), overwrite=True)
        add_option(env, "browsertime.secondary_url", test_site.get("secondary_url"))
        add_option(env, "browsertime.screenshot", "true")
        add_option(env, "browsertime.testName", test_site.get("name"))

        print("Recording %s to file: %s" % (test_site.get("url"), recording_file))
