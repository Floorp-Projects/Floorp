# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from mozperftest.browser.browsertime import add_options, add_option


sites = os.path.join(os.path.dirname(__file__), "sites.txt")
with open(sites) as f:
    sites = [site for site in f.read().split("\n") if site.strip()]


def next_site():
    for site in sites:
        yield site


get_site = next_site()

options = [
    ("firefox.preference", "network.http.speculative-parallel-limit:6"),
    ("firefox.preference", "gfx.webrender.force-disabled:true"),
    # XXX potentially move those as first class options in mozperf?
    ("pageCompleteWaitTime", "10000"),
    ("visualMetrics", "true"),
    ("video", "true"),
    ("firefox.windowRecorder", "false"),
    ("videoParams.addTimer", "false"),
    ("videoParams.createFilmstrip", "false"),
    ("videoParams.keepOriginalVideo", "true"),
]


def before_runs(env, **kw):
    env.set_arg("cycles", len(sites))
    add_options(env, options)


def before_cycle(env, **kw):
    url = next(get_site)
    add_option(env, "browsertime.url", url)
