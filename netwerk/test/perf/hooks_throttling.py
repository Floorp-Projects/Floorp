# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Drives the throttling feature when the test calls our
controlled server.
"""
import http.client
import json
import os
import sys
import time
from urllib.parse import urlparse

from mozperftest.test.browsertime import add_option
from mozperftest.utils import get_tc_secret

ENDPOINTS = {
    "linux": "h3.dev.mozaws.net",
    "darwin": "h3.mac.dev.mozaws.net",
    "win32": "h3.win.dev.mozaws.net",
}
CTRL_SERVER = ENDPOINTS[sys.platform]
TASK_CLUSTER = "TASK_ID" in os.environ.keys()
_SECRET = {
    "throttler_host": f"https://{CTRL_SERVER}/_throttler",
    "throttler_key": os.environ.get("WEBNETEM_KEY", ""),
}
if TASK_CLUSTER:
    _SECRET.update(get_tc_secret())

if _SECRET["throttler_key"] == "":
    if TASK_CLUSTER:
        raise Exception("throttler_key not found in secret")
    raise Exception("WEBNETEM_KEY not set")

_TIMEOUT = 30
WAIT_TIME = 60 * 10
IDLE_TIME = 10
BREATHE_TIME = 20


class Throttler:
    def __init__(self, env, host, key):
        self.env = env
        self.host = host
        self.key = key
        self.verbose = env.get_arg("verbose", False)
        self.logger = self.verbose and self.env.info or self.env.debug

    def log(self, msg):
        self.logger("[throttler] " + msg)

    def _request(self, action, data=None):
        kw = {}
        headers = {b"X-WEBNETEM-KEY": self.key}
        verb = data is None and "GET" or "POST"
        if data is not None:
            data = json.dumps(data)
            headers[b"Content-type"] = b"application/json"

        parsed = urlparse(self.host)
        server = parsed.netloc
        path = parsed.path
        if action != "status":
            path += "/" + action

        self.log(f"Calling {verb} {path}")
        conn = http.client.HTTPSConnection(server, timeout=_TIMEOUT)
        conn.request(verb, path, body=data, headers=headers, **kw)
        resp = conn.getresponse()
        res = resp.read()
        if resp.status >= 400:
            raise Exception(res)
        res = json.loads(res)
        return res

    def start(self, data=None):
        self.log("Starting")
        now = time.time()
        acquired = False

        while time.time() - now < WAIT_TIME:
            status = self._request("status")
            if status.get("test_running"):
                # a test is running
                self.log("A test is already controlling the server")
                self.log(f"Waiting {IDLE_TIME} seconds")
            else:
                try:
                    self._request("start_test")
                    acquired = True
                    break
                except Exception:
                    # we got beat in the race
                    self.log("Someone else beat us")
            time.sleep(IDLE_TIME)

        if not acquired:
            raise Exception("Could not acquire the test server")

        if data is not None:
            self._request("shape", data)

    def stop(self):
        self.log("Stopping")
        try:
            self._request("reset")
        finally:
            self._request("stop_test")


def get_throttler(env):
    host = _SECRET["throttler_host"]
    key = _SECRET["throttler_key"].encode()
    return Throttler(env, host, key)


_PROTOCOL = "h2", "h3"
_PAGE = "gallery", "news", "shopping", "photoblog"

# set the network condition here.
# each item has a name and some netem options:
#
# loss_ratio: specify percentage of packets that will be lost
# loss_corr: specify a correlation factor for the random packet loss
# dup_ratio: specify percentage of packets that will be duplicated
# delay: specify an overall delay for each packet
# jitter: specify amount of jitter in milliseconds
# delay_jitter_corr: specify a correlation factor for the random jitter
# reorder_ratio: specify percentage of packets that will be reordered
# reorder_corr: specify a correlation factor for the random reordering
#
_THROTTLING = (
    {"name": "full"},  # no throttling.
    {"name": "one", "delay": "20"},
    {"name": "two", "delay": "50"},
    {"name": "three", "delay": "100"},
    {"name": "four", "delay": "200"},
    {"name": "five", "delay": "300"},
)


def get_test():
    """Iterate on test conditions.

    For each cycle, we return a combination of: protocol, page, throttling
    settings. Each combination has a name, and that name will be used along with
    the protocol as a prefix for each metrics.
    """
    for proto in _PROTOCOL:
        for page in _PAGE:
            url = f"https://{CTRL_SERVER}/{page}.html"
            for throttler_settings in _THROTTLING:
                yield proto, page, url, throttler_settings


combo = get_test()


def before_cycle(metadata, env, cycle, script):
    global combo
    if "throttlable" not in script["tags"]:
        return
    throttler = get_throttler(env)
    try:
        proto, page, url, throttler_settings = next(combo)
    except StopIteration:
        combo = get_test()
        proto, page, url, throttler_settings = next(combo)

    # setting the url for the browsertime script
    add_option(env, "browsertime.url", url, overwrite=True)

    # enabling http if needed
    if proto == "h3":
        add_option(env, "firefox.preference", "network.http.http3.enable:true")

    # prefix used to differenciate metrics
    name = throttler_settings["name"]
    script["name"] = f"{name}_{proto}_{page}"

    # throttling the controlled server if needed
    if throttler_settings != {"name": "full"}:
        env.info("Calling the controlled server")
        throttler.start(throttler_settings)
    else:
        env.info("No throttling for this call")
        throttler.start()


def after_cycle(metadata, env, cycle, script):
    if "throttlable" not in script["tags"]:
        return
    throttler = get_throttler(env)
    try:
        throttler.stop()
    except Exception:
        pass

    # give a chance for a competitive job to take over
    time.sleep(BREATHE_TIME)
