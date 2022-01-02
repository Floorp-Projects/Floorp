#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division

from abc import ABCMeta, abstractmethod

import os
import json
import re
import six
import sys

import mozprocess
from manifestparser.util import evaluate_list_from_string
from benchmark import Benchmark
from logger.logger import RaptorLogger
from perftest import Perftest
from results import BrowsertimeResultsHandler

LOG = RaptorLogger(component="raptor-browsertime")

DEFAULT_CHROMEVERSION = "77"
BROWSERTIME_PAGELOAD_OUTPUT_TIMEOUT = 120  # 2 minutes
BROWSERTIME_BENCHMARK_OUTPUT_TIMEOUT = (
    None  # Disable output timeout for benchmark tests
)


@six.add_metaclass(ABCMeta)
class Browsertime(Perftest):
    """Abstract base class for Browsertime"""

    @property
    @abstractmethod
    def browsertime_args(self):
        pass

    def __init__(self, app, binary, process_handler=None, **kwargs):
        self.browsertime = True
        self.browsertime_failure = ""

        self.process_handler = process_handler or mozprocess.ProcessHandler
        for key in list(kwargs):
            if key.startswith("browsertime_"):
                value = kwargs.pop(key)
                setattr(self, key, value)

        def klass(**config):
            root_results_dir = os.path.join(
                os.environ.get("MOZ_UPLOAD_DIR", os.getcwd()), "browsertime-results"
            )
            return BrowsertimeResultsHandler(config, root_results_dir=root_results_dir)

        super(Browsertime, self).__init__(
            app, binary, results_handler_class=klass, **kwargs
        )
        LOG.info("cwd: '{}'".format(os.getcwd()))
        self.config["browsertime"] = True

        # Setup browsertime-specific settings for result parsing
        self.results_handler.browsertime_visualmetrics = self.browsertime_visualmetrics

        # For debugging.
        for k in (
            "browsertime_node",
            "browsertime_browsertimejs",
            "browsertime_ffmpeg",
            "browsertime_geckodriver",
            "browsertime_chromedriver",
        ):
            try:
                if not self.browsertime_video and k == "browsertime_ffmpeg":
                    continue
                LOG.info("{}: {}".format(k, getattr(self, k)))
                LOG.info("{}: {}".format(k, os.stat(getattr(self, k))))
            except Exception as e:
                LOG.info("{}: {}".format(k, e))

    def build_browser_profile(self):
        super(Browsertime, self).build_browser_profile()
        self.remove_mozprofile_delimiters_from_profile()

    def remove_mozprofile_delimiters_from_profile(self):
        # Perftest.build_browser_profile uses mozprofile to create the profile and merge in prefs;
        # while merging, mozprofile adds in special delimiters; these delimiters (along with blank
        # lines) are not recognized by selenium-webdriver ultimately causing Firefox launch to
        # fail. So we must remove these delimiters from the browser profile before passing into
        # btime via firefox.profileTemplate.

        LOG.info("Removing mozprofile delimiters from browser profile")
        userjspath = os.path.join(self.profile.profile, "user.js")
        try:
            with open(userjspath) as userjsfile:
                lines = userjsfile.readlines()
            lines = [
                line
                for line in lines
                if not line.startswith("#MozRunner") and line.strip()
            ]
            with open(userjspath, "w") as userjsfile:
                userjsfile.writelines(lines)
        except Exception as e:
            LOG.critical("Exception {} while removing mozprofile delimiters".format(e))

    def set_browser_test_prefs(self, raw_prefs):
        # add test specific preferences
        LOG.info("setting test-specific Firefox preferences")
        self.profile.set_preferences(json.loads(raw_prefs))
        self.remove_mozprofile_delimiters_from_profile()

    def run_test_setup(self, test):
        super(Browsertime, self).run_test_setup(test)

        if test.get("type") == "benchmark":
            # benchmark-type tests require the benchmark test to be served out
            self.benchmark = Benchmark(self.config, test)
            test["test_url"] = test["test_url"].replace("<host>", self.benchmark.host)
            test["test_url"] = test["test_url"].replace("<port>", self.benchmark.port)

        if test.get("playback") is not None and self.playback is None:
            self.start_playback(test)

        # TODO: geckodriver/chromedriver from tasks.
        self.driver_paths = []
        if self.browsertime_geckodriver:
            self.driver_paths.extend(
                ["--firefox.geckodriverPath", self.browsertime_geckodriver]
            )
        if self.browsertime_chromedriver and self.config["app"] in (
            "chrome",
            "chrome-m",
            "chromium",
        ):
            if (
                not self.config.get("run_local", None)
                or "{}" in self.browsertime_chromedriver
            ):
                if self.browser_version:
                    bvers = str(self.browser_version)
                    chromedriver_version = bvers.split(".")[0]
                else:
                    chromedriver_version = DEFAULT_CHROMEVERSION

                self.browsertime_chromedriver = self.browsertime_chromedriver.format(
                    chromedriver_version
                )

                if not os.path.exists(self.browsertime_chromedriver):
                    raise Exception(
                        "Cannot find the chromedriver for the chrome version "
                        "being tested: %s" % self.browsertime_chromedriver
                    )

            self.driver_paths.extend(
                ["--chrome.chromedriverPath", self.browsertime_chromedriver]
            )

        LOG.info("test: {}".format(test))

    def run_test_teardown(self, test):
        super(Browsertime, self).run_test_teardown(test)

        # if we were using a playback tool, stop it
        if self.playback is not None:
            self.playback.stop()
            self.playback = None

    def check_for_crashes(self):
        super(Browsertime, self).check_for_crashes()

    def clean_up(self):
        super(Browsertime, self).clean_up()

    def _compose_cmd(self, test, timeout):
        """Browsertime has the following overwrite priorities(in order of highest-lowest)
        (1) User - input commandline flag.
        (2) Browsertime args mentioned for a test
        (3) Test-manifest settings
        (4) Default settings
        """

        browsertime_path = os.path.join(
            os.path.dirname(__file__), "..", "..", "browsertime"
        )

        if test.get("type", "") == "scenario":
            browsertime_script = os.path.join(
                browsertime_path,
                "browsertime_scenario.js",
            )
        elif test.get("type", "") == "benchmark":
            browsertime_script = os.path.join(
                browsertime_path,
                "browsertime_benchmark.js",
            )
        else:
            # Custom scripts are treated as pageload tests for now
            if test.get("interactive", False):
                browsertime_script = os.path.join(
                    browsertime_path,
                    "browsertime_interactive.js",
                )
            else:
                browsertime_script = os.path.join(
                    browsertime_path,
                    test.get("test_script", "browsertime_pageload.js"),
                )

        page_cycle_delay = "1000"
        if self.config["live_sites"]:
            # Wait a bit longer when we run live site tests
            page_cycle_delay = "5000"

        # All the configurations in the browsertime_options variable initialization
        # and the secondary_url are priority 3, since none overlap they are grouped together
        browsertime_options = [
            "--firefox.noDefaultPrefs",
            "--browsertime.page_cycle_delay",
            page_cycle_delay,
            # Raptor's `pageCycleDelay` delay (ms) between pageload cycles
            "--skipHar",
            "--pageLoadStrategy",
            "none",
            "--webdriverPageload",
            "true",
            "--firefox.disableBrowsertimeExtension",
            "true",
            "--pageCompleteCheckStartWait",
            "5000",
            # url load timeout (milliseconds)
            "--pageCompleteCheckPollTimeout",
            "1000",
            # running browser scripts timeout (milliseconds)
            "--timeouts.pageLoad",
            str(timeout),
            "--timeouts.script",
            str(timeout * int(test.get("page_cycles", 1))),
            "--browsertime.page_cycles",
            str(test.get("page_cycles", 1)),
            # a delay was added by default to browsertime from 5s -> 8s for iphones, not needed
            "--pageCompleteWaitTime",
            str(test.get("page_complete_wait_time", "5000")),
            "--browsertime.url",
            test["test_url"],
            # Raptor's `post startup delay` is settle time after the browser has started
            "--browsertime.post_startup_delay",
            str(self.post_startup_delay),
        ]

        if test.get("secondary_url"):
            browsertime_options.extend(
                ["--browsertime.secondary_url", test.get("secondary_url")]
            )

        # In this code block we check if any priority 2 argument is in conflict with a priority
        # 3 arg if so we overwrite the value with the priority 2 argument, and otherwise we
        # simply add the priority 2 arg
        if test.get("browsertime_args", None):
            split_args = test.get("browsertime_args").strip().split()
            for split_arg in split_args:
                pairing = split_arg.split("=")
                if len(pairing) not in (1, 2):
                    raise Exception(
                        "One of the browsertime_args from the test was not split properly. "
                        f"Expecting a --flag, or a --option=value pairing. Found: {split_arg}"
                    )
                if pairing[0] in browsertime_options:
                    # If it's a flag, don't re-add it
                    if len(pairing) > 1:
                        ind = browsertime_options.index(pairing[0])
                        browsertime_options[ind + 1] = pairing[1]
                else:
                    browsertime_options.extend(pairing)

        priority1_options = self.browsertime_args
        if self.config["app"] in ("chrome", "chromium", "chrome-m"):
            priority1_options.extend(self.setup_chrome_args(test))

        # must happen before --firefox.profileTemplate and --resultDir
        self.results_handler.remove_result_dir_for_test(test)
        priority1_options.extend(
            ["--firefox.profileTemplate", str(self.profile.profile)]
        )
        priority1_options.extend(
            ["--resultDir", self.results_handler.result_dir_for_test(test)]
        )

        # This argument can have duplicates of the value "--firefox.env" so we do not need
        # to check if it conflicts
        for var, val in self.config.get("environment", {}).items():
            browsertime_options.extend(["--firefox.env", "{}={}".format(var, val)])

        # Parse the test commands (if any) from the test manifest
        cmds = evaluate_list_from_string(test.get("test_cmds", "[]"))
        parsed_cmds = [":::".join([str(i) for i in item]) for item in cmds if item]
        browsertime_options.extend(["--browsertime.commands", ";;;".join(parsed_cmds)])

        if self.verbose and "-vvv" not in browsertime_options:
            browsertime_options.append("-vvv")

        if self.browsertime_video:
            priority1_options.extend(
                [
                    "--video",
                    "true",
                    "--visualMetrics",
                    "true" if self.browsertime_visualmetrics else "false",
                ]
            )

            if self.browsertime_no_ffwindowrecorder or self.config["app"] in (
                "chromium",
                "chrome-m",
                "chrome",
            ):
                priority1_options.extend(
                    [
                        "--firefox.windowRecorder",
                        "false",
                        "--xvfbParams.display",
                        "0",
                    ]
                )
                LOG.info(
                    "Using adb screenrecord for mobile, or ffmpeg on desktop for videos"
                )
            else:
                priority1_options.extend(
                    [
                        "--firefox.windowRecorder",
                        "true",
                    ]
                )
                LOG.info("Using Firefox Window Recorder for videos")
        else:
            priority1_options.extend(["--video", "false", "--visualMetrics", "false"])

        # have browsertime use our newly-created conditioned-profile path
        if self.config.get("conditioned_profile"):
            self.profile.profile = self.conditioned_profile_dir

        if self.config["gecko_profile"]:
            self.config[
                "browsertime_result_dir"
            ] = self.results_handler.result_dir_for_test(test)
            self._init_gecko_profiling(test)
            priority1_options.append("--firefox.geckoProfiler")
            for option, browser_time_option, default in (
                (
                    "gecko_profile_features",
                    "--firefox.geckoProfilerParams.features",
                    "js,leaf,stackwalk,cpu,threads",
                ),
                (
                    "gecko_profile_threads",
                    "--firefox.geckoProfilerParams.threads",
                    "GeckoMain,Compositor",
                ),
                (
                    "gecko_profile_interval",
                    "--firefox.geckoProfilerParams.interval",
                    None,
                ),
                (
                    "gecko_profile_entries",
                    "--firefox.geckoProfilerParams.bufferSize",
                    None,
                ),
            ):
                # 0 is a valid value. The setting may be present but set to None.
                value = self.config.get(option)
                if value is None:
                    value = test.get(option)
                if value is None:
                    value = default
                if option == "gecko_profile_threads":
                    extra = self.config.get("gecko_profile_extra_threads", [])
                    value = ",".join(value.split(",") + extra)
                if value is not None:
                    priority1_options.extend([browser_time_option, str(value)])

        # In this code block we check if any priority 1 arguments are in conflict with a
        # priority 2/3/4 argument
        MULTI_OPTS = [
            "--firefox.android.intentArgument",
        ]
        for index, argument in list(enumerate(priority1_options)):
            if argument in MULTI_OPTS:
                browsertime_options.extend([argument, priority1_options[index + 1]])
            elif argument.startswith("--"):
                if index == len(priority1_options) - 1:
                    if argument not in browsertime_options:
                        browsertime_options.append(argument)
                else:
                    arg = priority1_options[index + 1]
                    try:
                        ind = browsertime_options.index(argument)
                        if not arg.startswith("--"):
                            browsertime_options[ind + 1] = arg
                    except ValueError:
                        res = [argument]
                        if not arg.startswith("--"):
                            res.append(arg)
                        browsertime_options.extend(res)
            else:
                continue

        return (
            [self.browsertime_node, self.browsertime_browsertimejs]
            + self.driver_paths
            + [browsertime_script]
            + browsertime_options
            # -n option for the browsertime to restart the browser
            + ["-n", str(test.get("browser_cycles", 1))]
        )

    def _compute_process_timeout(self, test, timeout):
        # bt_timeout will be the overall browsertime cmd/session timeout (seconds)
        # browsertime deals with page cycles internally, so we need to give it a timeout
        # value that includes all page cycles
        # pylint --py3k W1619
        bt_timeout = int(timeout / 1000) * int(test.get("page_cycles", 1))

        # the post-startup-delay is a delay after the browser has started, to let it settle
        # it's handled within browsertime itself by loading a 'preUrl' (about:blank) and having a
        # delay after that ('preURLDelay') as the post-startup-delay, so we must add that in sec
        # pylint --py3k W1619
        bt_timeout += int(self.post_startup_delay / 1000)

        # add some time for browser startup, time for the browsertime measurement code
        # to be injected/invoked, and for exceptions to bubble up; be generous
        bt_timeout += 20

        # browsertime also handles restarting the browser/running all of the browser cycles;
        # so we need to multiply our bt_timeout by the number of browser cycles
        bt_timeout = bt_timeout * int(test.get("browser_cycles", 1))

        # if geckoProfile enabled, give browser more time for profiling
        if self.config["gecko_profile"] is True:
            bt_timeout += 5 * 60
        return bt_timeout

    def run_test(self, test, timeout):
        global BROWSERTIME_PAGELOAD_OUTPUT_TIMEOUT

        self.run_test_setup(test)
        # timeout is a single page-load timeout value (ms) from the test INI
        # this will be used for btime --timeouts.pageLoad
        cmd = self._compose_cmd(test, timeout)

        if test.get("type", "") == "scenario":
            # Change the timeout for scenarios since they
            # don't output much for a long period of time
            BROWSERTIME_PAGELOAD_OUTPUT_TIMEOUT = timeout

        LOG.info("timeout (s): {}".format(timeout))
        LOG.info("browsertime cwd: {}".format(os.getcwd()))
        LOG.info("browsertime cmd: {}".format(" ".join([str(c) for c in cmd])))
        if self.browsertime_video:
            LOG.info("browsertime_ffmpeg: {}".format(self.browsertime_ffmpeg))

        # browsertime requires ffmpeg on the PATH for `--video=true`.
        # It's easier to configure the PATH here than at the TC level.
        env = dict(os.environ)
        if self.browsertime_video and self.browsertime_ffmpeg:
            ffmpeg_dir = os.path.dirname(os.path.abspath(self.browsertime_ffmpeg))
            old_path = env.setdefault("PATH", "")
            new_path = os.pathsep.join([ffmpeg_dir, old_path])
            if isinstance(new_path, six.text_type):
                # Python 2 doesn't like unicode in the environment.
                new_path = new_path.encode("utf-8", "strict")
            env["PATH"] = new_path

        LOG.info("PATH: {}".format(env["PATH"]))

        try:
            line_matcher = re.compile(r".*(\[.*\])\s+([a-zA-Z]+):\s+(.*)")

            def _line_handler(line):
                """This function acts as a bridge between browsertime
                and raptor. It reforms the lines to get rid of information
                that is not needed, and outputs them appropriately based
                on the level that is found. (Debug and info all go to info).

                For errors, we set an attribute (self.browsertime_failure) to
                it, then raise a generic exception. When we return, we check
                if self.browsertime_failure, and raise an Exception if necessary
                to stop Raptor execution (preventing the results processing).
                """

                # NOTE: this hack is to workaround encoding issues on windows
                # a newer version of browsertime adds a `σ` character to output
                line = line.replace(b"\xcf\x83", b"")

                line = line.decode("utf-8")
                match = line_matcher.match(line)
                if not match:
                    LOG.info(line)
                    return

                date, level, msg = match.groups()
                level = level.lower()
                if "error" in level:
                    self.browsertime_failure = msg
                    LOG.error("Browsertime failed to run")
                    proc.kill()
                elif "warning" in level:
                    LOG.warning(msg)
                elif "metrics" in level:
                    vals = msg.split(":")[-1].strip()
                    self.page_count = vals.split(",")
                else:
                    LOG.info(msg)

            if self.browsertime_visualmetrics and self.run_local:
                # Check if visual metrics is installed correctly before running the test
                self.vismet_failed = False

                def _vismet_line_handler(line):
                    line = line.decode("utf-8")
                    LOG.info(line)
                    if "FAIL" in line:
                        self.vismet_failed = True

                proc = self.process_handler(
                    [sys.executable, self.browsertime_vismet_script, "--check"],
                    processOutputLine=_vismet_line_handler,
                    env=env,
                )
                proc.run()
                proc.wait()

                if self.vismet_failed:
                    raise Exception(
                        "Browsertime visual metrics dependencies were not "
                        "installed correctly. Try removing the virtual environment at "
                        "%s before running your command again."
                        % os.environ["VIRTUAL_ENV"]
                    )

            proc = self.process_handler(cmd, processOutputLine=_line_handler, env=env)
            proc.run(
                timeout=self._compute_process_timeout(test, timeout),
                outputTimeout=BROWSERTIME_BENCHMARK_OUTPUT_TIMEOUT
                if self.benchmark
                else BROWSERTIME_PAGELOAD_OUTPUT_TIMEOUT,
            )
            proc.wait()

            if self.browsertime_failure:
                raise Exception(self.browsertime_failure)

        except Exception as e:
            LOG.critical(str(e))
            raise
