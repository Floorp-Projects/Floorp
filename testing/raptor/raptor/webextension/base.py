#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import json
import os
import requests
import time

from benchmark import Benchmark
from cmdline import CHROMIUM_DISTROS
from control_server import RaptorControlServer
from gen_test_config import gen_test_config
from logger.logger import RaptorLogger
from memory import generate_android_memory_profile
from perftest import Perftest
from results import RaptorResultsHandler

LOG = RaptorLogger(component="raptor-webext")

here = os.path.abspath(os.path.dirname(__file__))
webext_dir = os.path.join(here, "..", "..", "webext")


class WebExtension(Perftest):
    """Container class for WebExtension."""

    def __init__(self, *args, **kwargs):
        self.raptor_webext = None
        self.control_server = None
        self.cpu_profiler = None

        super(WebExtension, self).__init__(*args, **kwargs)

        # set up the results handler
        self.results_handler = RaptorResultsHandler(
            gecko_profile=self.config.get("gecko_profile"),
            power_test=self.config.get("power_test"),
            cpu_test=self.config.get("cpu_test"),
            memory_test=self.config.get("memory_test"),
            no_conditioned_profile=self.config["no_conditioned_profile"],
            extra_prefs=self.config.get("extra_prefs"),
        )
        browser_name, browser_version = self.get_browser_meta()
        self.results_handler.add_browser_meta(self.config["app"], browser_version)

        self.start_control_server()

    def run_test_setup(self, test):
        super(WebExtension, self).run_test_setup(test)

        LOG.info("starting web extension test: %s" % test["name"])
        LOG.info("test settings: %s" % str(test))
        LOG.info("web extension config: %s" % str(self.config))

        if test.get("type") == "benchmark":
            self.serve_benchmark_source(test)

        gen_test_config(
            self.config["app"],
            test["name"],
            self.control_server.port,
            self.post_startup_delay,
            host=self.config["host"],
            b_port=int(self.benchmark.port) if self.benchmark else 0,
            debug_mode=1 if self.debug_mode else 0,
            browser_cycle=test.get("browser_cycle", 1),
        )

        self.install_raptor_webext()

    def wait_for_test_finish(self, test, timeout):
        # this is a 'back-stop' i.e. if for some reason Raptor doesn't finish for some
        # serious problem; i.e. the test was unable to send a 'page-timeout' to the control
        # server, etc. Therefore since this is a 'back-stop' we want to be generous here;
        # we don't want this timeout occurring unless abosultely necessary

        # convert timeout to seconds and account for page cycles
        timeout = int(timeout / 1000) * int(test.get("page_cycles", 1))
        # account for the pause the raptor webext runner takes after browser startup
        # and the time an exception is propagated through the framework
        timeout += int(self.post_startup_delay / 1000) + 10

        # for page-load tests we don't start the page-timeout timer until the pageload.js content
        # is successfully injected and invoked; which differs per site being tested; therefore we
        # need to be generous here - let's add 10 seconds extra per page-cycle
        if test.get("type") == "pageload":
            timeout += 10 * int(test.get("page_cycles", 1))

        # if geckoProfile enabled, give browser more time for profiling
        if self.config["gecko_profile"] is True:
            timeout += 5 * 60

        # we also need to give time for results processing, not just page/browser cycles!
        timeout += 60

        elapsed_time = 0
        while not self.control_server._finished:
            if self.config["enable_control_server_wait"]:
                response = self.control_server_wait_get()
                if response == "webext_shutdownBrowser":
                    if self.config["memory_test"]:
                        generate_android_memory_profile(self, test["name"])
                    if self.cpu_profiler:
                        self.cpu_profiler.generate_android_cpu_profile(test["name"])

                    self.control_server_wait_continue()
            time.sleep(1)
            # we only want to force browser-shutdown on timeout if not in debug mode;
            # in debug-mode we leave the browser running (require manual shutdown)
            if not self.debug_mode:
                elapsed_time += 1
                if elapsed_time > (timeout) - 5:  # stop 5 seconds early
                    self.control_server.wait_for_quit()

                    if not self.control_server.is_webextension_loaded:
                        raise RuntimeError("Connection to Raptor webextension failed!")

                    raise RuntimeError(
                        "Test failed to finish. "
                        "Application timed out after {} seconds".format(timeout)
                    )

        if self.control_server._runtime_error:
            raise RuntimeError("Failed to run {}: {}\nStack:\n{}".format(
                test["name"],
                self.control_server._runtime_error["error"],
                self.control_server._runtime_error["stack"],
            ))

    def run_test_teardown(self, test):
        super(WebExtension, self).run_test_teardown(test)

        if self.playback is not None:
            self.playback.stop()

            confidence_values = self.playback.confidence()
            if confidence_values:
                mozproxy_replay = {
                    u'type': u'mozproxy-replay',
                    u'test': test["name"],
                    u'unit': u'a.u.',
                    u'values': confidence_values
                }
                self.control_server.submit_supporting_data(mozproxy_replay)
            else:
                LOG.info("Mozproxy replay confidence data not available!")

            self.playback = None

        self.remove_raptor_webext()

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        LOG.info("setting test-specific Firefox preferences")
        self.profile.set_preferences(json.loads(raw_prefs))

    def build_browser_profile(self):
        super(WebExtension, self).build_browser_profile()

        if self.control_server:
            # The control server and the browser profile are not well factored
            # at this time, so the start-up process overlaps.  Accommodate.
            self.control_server.user_profile = self.profile

    def start_control_server(self):
        self.control_server = RaptorControlServer(
            self.results_handler,
            self.debug_mode
        )
        self.control_server.user_profile = self.profile
        self.control_server.start()

        if self.config["enable_control_server_wait"]:
            self.control_server_wait_set("webext_shutdownBrowser")

    def serve_benchmark_source(self, test):
        # benchmark-type tests require the benchmark test to be served out
        self.benchmark = Benchmark(self.config, test)

    def install_raptor_webext(self):
        # must intall raptor addon each time because we dynamically update some content
        # the webext is installed into the browser profile
        # note: for chrome the addon is just a list of paths that ultimately are added
        # to the chromium command line '--load-extension' argument
        self.raptor_webext = os.path.join(webext_dir, "raptor")
        LOG.info("installing webext %s" % self.raptor_webext)
        self.profile.addons.install(self.raptor_webext)

        # on firefox we can get an addon id; chrome addon actually is just cmd line arg
        try:
            self.webext_id = self.profile.addons.addon_details(self.raptor_webext)["id"]
        except AttributeError:
            self.webext_id = None

        self.control_server.startup_handler(False)

    def remove_raptor_webext(self):
        # remove the raptor webext; as it must be reloaded with each subtest anyway
        if not self.raptor_webext:
            LOG.info("raptor webext not installed - not attempting removal")
            return

        LOG.info("removing webext %s" % self.raptor_webext)
        if self.config["app"] in ["firefox", "geckoview", "fennec", "refbrow", "fenix"]:
            self.profile.addons.remove_addon(self.webext_id)

        # for chrome the addon is just a list (appended to cmd line)
        chrome_apps = CHROMIUM_DISTROS + ["chrome-android", "chromium-android"]
        if self.config["app"] in chrome_apps:
            self.profile.addons.remove(self.raptor_webext)

        self.raptor_webext = None

    def clean_up(self):
        super(WebExtension, self).clean_up()

        if self.config["enable_control_server_wait"]:
            self.control_server_wait_clear("all")

        self.control_server.stop()
        LOG.info("finished")

    def control_server_wait_set(self, state):
        response = requests.post(
            "http://127.0.0.1:%s/" % self.control_server.port,
            json={"type": "wait-set", "data": state},
        )
        return response.text

    def control_server_wait_timeout(self, timeout):
        response = requests.post(
            "http://127.0.0.1:%s/" % self.control_server.port,
            json={"type": "wait-timeout", "data": timeout},
        )
        return response.text

    def control_server_wait_get(self):
        response = requests.post(
            "http://127.0.0.1:%s/" % self.control_server.port,
            json={"type": "wait-get", "data": ""},
        )
        return response.text

    def control_server_wait_continue(self):
        response = requests.post(
            "http://127.0.0.1:%s/" % self.control_server.port,
            json={"type": "wait-continue", "data": ""},
        )
        return response.text

    def control_server_wait_clear(self, state):
        response = requests.post(
            "http://127.0.0.1:%s/" % self.control_server.port,
            json={"type": "wait-clear", "data": state},
        )
        return response.text
