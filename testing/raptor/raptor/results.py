# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# class to process, format, and report raptor test results
# received from the raptor control server
from __future__ import absolute_import

import json
import os

from abc import ABCMeta, abstractmethod
from logger.logger import RaptorLogger
from output import RaptorOutput, BrowsertimeOutput

LOG = RaptorLogger(component="perftest-results-handler")


class PerftestResultsHandler(object):
    """Abstract base class to handle perftest results"""

    __metaclass__ = ABCMeta

    def __init__(
        self,
        gecko_profile=False,
        power_test=False,
        cpu_test=False,
        memory_test=False,
        app=None,
        no_conditioned_profile=False,
        **kwargs
    ):
        self.gecko_profile = gecko_profile
        self.power_test = power_test
        self.cpu_test = cpu_test
        self.memory_test = memory_test
        self.app = app
        self.results = []
        self.page_timeout_list = []
        self.images = []
        self.supporting_data = None
        self.fission_enabled = kwargs.get("extra_prefs", {}).get(
            "fission.autostart", False
        )
        self.browser_version = None
        self.browser_name = None
        self.no_conditioned_profile = no_conditioned_profile

    @abstractmethod
    def add(self, new_result_json):
        raise NotImplementedError()

    def add_browser_meta(self, browser_name, browser_version):
        # sets the browser metadata for the perfherder data
        self.browser_name = browser_name
        self.browser_version = browser_version

    def add_image(self, screenshot, test_name, page_cycle):
        # add to results
        LOG.info("received screenshot")
        self.images.append(
            {"screenshot": screenshot, "test_name": test_name, "page_cycle": page_cycle}
        )

    def add_page_timeout(self, test_name, page_url, pending_metrics):
        timeout_details = {"test_name": test_name, "url": page_url}
        if pending_metrics:
            pending_metrics = [key for key, value in pending_metrics.items() if value]
            timeout_details["pending_metrics"] = ", ".join(pending_metrics)

        self.page_timeout_list.append(timeout_details)

    def add_supporting_data(self, supporting_data):
        """ Supporting data is additional data gathered outside of the regular
        Raptor test run (i.e. power data). Will arrive in a dict in the format of:

        supporting_data = {'type': 'data-type',
                           'test': 'raptor-test-ran-when-data-was-gathered',
                           'unit': 'unit that the values are in',
                           'values': {
                               'name': value,
                               'nameN': valueN}}

        More specifically, power data will look like this:

        supporting_data = {'type': 'power',
                           'test': 'raptor-speedometer-geckoview',
                           'unit': 'mAh',
                           'values': {
                               'cpu': cpu,
                               'wifi': wifi,
                               'screen': screen,
                               'proportional': proportional}}
        """
        LOG.info(
            "RaptorResultsHandler.add_supporting_data received %s data"
            % supporting_data["type"]
        )
        if self.supporting_data is None:
            self.supporting_data = []
        self.supporting_data.append(supporting_data)

    def _get_expected_perfherder(self, output):
        def is_resource_test():
            if self.power_test or self.cpu_test or self.memory_test:
                return True
            return False

        # if results exists, determine if any test is of type 'scenario'
        is_scenario = False
        if output.summarized_results or output.summarized_supporting_data:
            data = output.summarized_supporting_data
            if not data:
                data = [output.summarized_results]
            for next_data_set in data:
                data_type = next_data_set["suites"][0]["type"]
                if data_type == "scenario":
                    is_scenario = True
                    break

        if is_scenario and not is_resource_test():
            # skip perfherder check when a scenario test-type is run without
            # a resource flag
            return None

        expected_perfherder = 1

        if output.mozproxy_data:
            # Check if we have mozproxy data available.
            expected_perfherder += 1

        if is_resource_test():
            # when resource tests are run, no perfherder data is output
            # for the regular raptor tests (i.e. speedometer) so we
            # expect one per resource-type, starting with 0
            expected_perfherder = 0
            if self.power_test:
                expected_perfherder += 1
            if self.memory_test:
                expected_perfherder += 1
            if self.cpu_test:
                expected_perfherder += 1

        return expected_perfherder

    def _validate_treeherder_data(self, output, output_perfdata):
        # late import is required, because install is done in create_virtualenv
        import jsonschema

        expected_perfherder = self._get_expected_perfherder(output)
        if expected_perfherder is None:
            LOG.info(
                "Skipping PERFHERDER_DATA check "
                "because no perfherder data output is expected"
            )
            return True
        elif output_perfdata != expected_perfherder:
            LOG.critical(
                "PERFHERDER_DATA was seen %d times, expected %d."
                % (output_perfdata, expected_perfherder)
            )
            return False

        external_tools_path = os.environ["EXTERNALTOOLSPATH"]
        schema_path = os.path.join(
            external_tools_path, "performance-artifact-schema.json"
        )
        LOG.info("Validating PERFHERDER_DATA against %s" % schema_path)
        try:
            with open(schema_path) as f:
                schema = json.load(f)
            if output.summarized_results:
                data = output.summarized_results
            else:
                data = output.summarized_supporting_data[0]
            jsonschema.validate(data, schema)
        except Exception as e:
            LOG.exception("Error while validating PERFHERDER_DATA")
            LOG.info(str(e))
            return False
        return True

    @abstractmethod
    def summarize_and_output(self, test_config, tests, test_names):
        raise NotImplementedError()


class RaptorResultsHandler(PerftestResultsHandler):
    """Process Raptor results"""

    def add(self, new_result_json):
        # add to results
        if new_result_json.get("extra_options") is None:
            new_result_json["extra_options"] = []
        LOG.info("received results in RaptorResultsHandler.add")
        if self.no_conditioned_profile:
            new_result_json["extra_options"].append("nocondprof")
        if self.fission_enabled:
            new_result_json["extra_options"].append("fission")
        self.results.append(new_result_json)

    def summarize_and_output(self, test_config, tests, test_names):
        # summarize the result data, write to file and output PERFHERDER_DATA
        LOG.info("summarizing raptor test results")
        output = RaptorOutput(
            self.results, self.supporting_data, test_config["subtest_alert_on"]
        )
        output.set_browser_meta(self.browser_name, self.browser_version)
        output.summarize(test_names)
        # that has each browser cycle separate; need to check if there were multiple browser
        # cycles, and if so need to combine results from all cycles into one overall result
        output.combine_browser_cycles()
        output.summarize_screenshots(self.images)

        # only dump out supporting data (i.e. power) if actual Raptor test completed
        out_sup_perfdata = 0
        sup_success = True
        if self.supporting_data is not None and len(self.results) != 0:
            output.summarize_supporting_data()
            sup_success, out_sup_perfdata = output.output_supporting_data(test_names)

        success, out_perfdata = output.output(test_names)

        validate_success = True
        if not self.gecko_profile:
            validate_success = self._validate_treeherder_data(
                output, out_sup_perfdata + out_perfdata
            )

        return sup_success and success and validate_success and not self.page_timeout_list


class BrowsertimeResultsHandler(PerftestResultsHandler):
    """Process Browsertime results"""

    def __init__(self, config, root_results_dir=None):
        super(BrowsertimeResultsHandler, self).__init__(**config)
        self._root_results_dir = root_results_dir

    def result_dir(self):
        return self._root_results_dir

    def result_dir_for_test(self, test):
        return os.path.join(self._root_results_dir, test["name"])

    def add(self, new_result_json):
        # not using control server with bt
        pass

    def parse_browsertime_json(
        self, raw_btresults, page_cycles, cold, browser_cycles, measure
    ):
        """
        Receive a json blob that contains the results direct from the browsertime tool. Parse
        out the values that we wish to use and add those to our result object. That object will
        then be further processed in the BrowsertimeOutput class.

        The values that we care about in the browsertime.json are structured as follows.
        The 'browserScripts' section has one entry for each page-load / browsertime cycle!

        [
          {
            "info": {
              "browsertime": {
                "version": "4.9.2-android"
              },
              "url": "https://www.theguardian.co.uk",
            },
            "browserScripts": [
              {
                "browser": {
                  "userAgent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.13; rv:70.0)
                                Gecko/20100101 Firefox/70.0",
                  "windowSize": "1366x694"
                },
                "timings": {
                  "firstPaint": 830,
                  "loadEventEnd": 4450,
                  "timeToContentfulPaint": 932,
                  "timeToDomContentFlushed": 864,
                  }
                }
              },
              {
                <repeated for every page-load cycle>
              },
            ],
            "statistics": {
              "timings": {
                "firstPaint": {
                  "median": 668,
                  "mean": 680,
                  "mdev": 9.6851,
                  "stddev": 48,
                  "min": 616,
                  "p10": 642,
                  "p90": 719,
                  "p99": 830,
                  "max": 830
                },
                "loadEventEnd": {
                  "median": 3476,
                  "mean": 3642,
                  "mdev": 111.7028,
                  "stddev": 559,
                  "min": 3220,
                  "p10": 3241,
                  "p90": 4450,
                  "p99": 5818,
                  "max": 5818
                },
                "timeToContentfulPaint": {
                  "median": 758,
                  "mean": 769,
                  "mdev": 10.0941,
                  "stddev": 50,
                  "min": 712,
                  "p10": 728,
                  "p90": 810,
                  "p99": 932,
                  "max": 932
                },
                "timeToDomContentFlushed": {
                  "median": 670,
                  "mean": 684,
                  "mdev": 11.6768,
                  "stddev": 58,
                  "min": 614,
                  "p10": 632,
                  "p90": 738,
                  "p99": 864,
                  "max": 864
                },
              }
            }
          }
        ]

        For benchmark tests the browserScripts tag will look like:
        "browserScripts":[
         {
            "browser":{ },
            "pageinfo":{ },
            "timings":{ },
            "custom":{
               "benchmarks":{
                  "speedometer":[
                     {
                        "Angular2-TypeScript-TodoMVC":[
                           326.41999999999825,
                           238.7799999999952,
                           211.88000000000463,
                           186.77999999999884,
                           191.47999999999593
                        ],
                        etc...
                    }
                  ]
               }
           }
        """
        LOG.info("parsing results from browsertime json")

        # bt to raptor names
        conversion = (
            ("fnbpaint", "firstPaint"),
            ("fcp", "timeToContentfulPaint"),
            ("dcf", "timeToDomContentFlushed"),
            ("loadtime", "loadEventEnd"),
        )

        chrome_raptor_conversion = {
            "timeToContentfulPaint": ["paintTiming", "first-contentful-paint"]
        }

        def _get_raptor_val(mdict, mname, retval=False):
            # gets the measurement requested, returns the value
            # if one was found, or retval if it couldn't be found
            #
            # mname: either a path to follow (as a list) to get to
            #        a requested field value, or a string to check
            #        if mdict contains it. i.e.
            #        'first-contentful-paint'/'fcp' is found by searching
            #        in mdict['paintTiming'].
            # mdict: a dictionary to look through to find the mname
            #        value.

            if type(mname) != list:
                if mname in mdict:
                    return mdict[mname]
                return retval
            target = mname[-1]
            tmpdict = mdict
            for name in mname[:-1]:
                tmpdict = tmpdict.get(name, {})
            if target in tmpdict:
                return tmpdict[target]

            return retval

        results = []

        # Do some preliminary results validation. When running cold page-load, the results will
        # be all in one entry already, as browsertime groups all cold page-load iterations in
        # one results entry with all replicates within. When running warm page-load, there will
        # be one results entry for every warm page-load iteration; with one single replicate
        # inside each.
        if cold:
            if len(raw_btresults) == 0:
                raise MissingResultsError(
                    "Missing results for all cold browser cycles."
                )
        else:
            if len(raw_btresults) != int(page_cycles):
                raise MissingResultsError(
                    "Missing results for at least 1 warm page-cycle."
                )

        # now parse out the values
        for raw_result in raw_btresults:
            if not raw_result["browserScripts"]:
                raise MissingResultsError("Browsertime cycle produced no measurements.")

            if raw_result["browserScripts"][0].get("timings") is None:
                raise MissingResultsError("Browsertime cycle is missing all timings")

            # Desktop chrome doesn't have `browser` scripts data available for now
            bt_browser = raw_result["browserScripts"][0].get("browser", None)
            bt_ver = raw_result["info"]["browsertime"]["version"]
            bt_url = (raw_result["info"]["url"],)
            bt_result = {
                "bt_ver": bt_ver,
                "browser": bt_browser,
                "url": bt_url,
                "measurements": {},
                "statistics": {},
            }

            custom_types = raw_result["browserScripts"][0].get("custom")
            if custom_types:
                for custom_type in custom_types:
                    bt_result["measurements"].update(
                        {k: [v] for k, v in custom_types[custom_type].items()}
                    )
            else:
                # extracting values from browserScripts and statistics
                for bt, raptor in conversion:
                    if measure is not None and bt not in measure:
                        continue
                    # chrome we just measure fcp and loadtime; skip fnbpaint and dcf
                    if (
                        self.app
                        and "chrome" in self.app.lower()
                        and bt in ("fnbpaint", "dcf")
                    ):
                        continue
                    # fennec doesn't support 'fcp'
                    if self.app and "fennec" in self.app.lower() and bt == "fcp":
                        continue

                    # chrome currently uses different names (and locations) for some metrics
                    if raptor in chrome_raptor_conversion and _get_raptor_val(
                        raw_result["browserScripts"][0]["timings"],
                        chrome_raptor_conversion[raptor],
                    ):
                        raptor = chrome_raptor_conversion[raptor]

                    # XXX looping several times in the list, could do better
                    for cycle in raw_result["browserScripts"]:
                        if bt not in bt_result["measurements"]:
                            bt_result["measurements"][bt] = []
                        val = _get_raptor_val(cycle["timings"], raptor)
                        if not val:
                            raise MissingResultsError(
                                "Browsertime cycle missing {} measurement".format(
                                    raptor
                                )
                            )
                        bt_result["measurements"][bt].append(val)

                    # let's add the browsertime statistics; we'll use those for overall values
                    # instead of calculating our own based on the replicates
                    bt_result["statistics"][bt] = _get_raptor_val(
                        raw_result["statistics"]["timings"], raptor, retval={}
                    )

            results.append(bt_result)

        return results

    def _extract_vmetrics(self, test_name, browsertime_json):
        # The visual metrics task expects posix paths.
        def _normalized_join(*args):
            path = os.path.join(*args)
            return path.replace(os.path.sep, "/")

        name = browsertime_json.split(os.path.sep)[-2]
        reldir = _normalized_join("browsertime-results", name)

        return {
            "browsertime_json_path": _normalized_join(reldir, "browsertime.json"),
            "test_name": test_name,
        }

    def summarize_and_output(self, test_config, tests, test_names):
        """
        Retrieve, process, and output the browsertime test results. Currently supports page-load
        type tests only.

        The Raptor framework either ran a single page-load test (one URL) - or - an entire suite
        of page-load tests (multiple test URLs). Regardless, every test URL measured will
        have its own 'browsertime.json' results file, located in a sub-folder names after the
        Raptor test name, i.e.:

        browsertime-results/
            raptor-tp6-amazon-firefox
                browsertime.json
            raptor-tp6-facebook-firefox
                browsertime.json
            raptor-tp6-google-firefox
                browsertime.json
            raptor-tp6-youtube-firefox
                browsertime.json

        For each test URL that was measured, find the resulting 'browsertime.json' file, and pull
        out the values that we care about.
        """
        # summarize the browsertime result data, write to file and output PERFHERDER_DATA
        LOG.info("retrieving browsertime test results")

        # video_jobs is populated with video files produced by browsertime, we
        # will send to the visual metrics task
        video_jobs = []
        run_local = test_config.get("run_local", False)

        for test in tests:
            test_name = test["name"]
            bt_res_json = os.path.join(
                self.result_dir_for_test(test), "browsertime.json"
            )
            if os.path.exists(bt_res_json):
                LOG.info("found browsertime results at %s" % bt_res_json)
            else:
                LOG.critical("unable to find browsertime results at %s" % bt_res_json)
                return False

            try:
                with open(bt_res_json, "r") as f:
                    raw_btresults = json.load(f)
            except Exception as e:
                LOG.error("Exception reading %s" % bt_res_json)
                # XXX this should be replaced by a traceback call
                LOG.error("Exception: %s %s" % (type(e).__name__, str(e)))
                raise

            if not run_local:
                video_jobs.append(self._extract_vmetrics(test_name, bt_res_json))

            for new_result in self.parse_browsertime_json(
                raw_btresults,
                test["page_cycles"],
                test["cold"],
                test["browser_cycles"],
                test.get("measure"),
            ):

                def _new_pageload_result(new_result):
                    # add additional info not from the browsertime json
                    for field in (
                        "name",
                        "unit",
                        "lower_is_better",
                        "alert_threshold",
                        "cold",
                    ):
                        new_result[field] = test[field]

                    # Differentiate Raptor `pageload` tests from `browsertime-pageload`
                    # tests while we compare and contrast.
                    new_result["type"] = "browsertime-pageload"

                    # All Browsertime measurements are elapsed times in milliseconds.
                    new_result["subtest_lower_is_better"] = True
                    new_result["subtest_unit"] = "ms"
                    LOG.info("parsed new result: %s" % str(new_result))

                    # `extra_options` will be populated with Gecko profiling flags in
                    # the future.
                    new_result["extra_options"] = []
                    if self.no_conditioned_profile:
                        new_result["extra_options"].append("nocondprof")
                    if self.fission_enabled:
                        new_result["extra_options"].append("fission")

                    # TODO: Once Bug 1593198 is fixed remove this part
                    # Currently perfherder doesn't split data / recognize 'application` and for
                    # browsertime tests we don't use the browser name in the test name as we use
                    # simplified test INIs; therefore in order to differentiate in perfherder
                    # between browsers/apps, until Bug 1593198 is fixed, we must add the app name
                    # to the perfherder data extraOptions fields
                    if self.app != "firefox":
                        new_result["extra_options"].append(self.app)

                    return new_result

                def _new_benchmark_result(new_result):
                    # add additional info not from the browsertime json
                    for field in (
                        "name",
                        "unit",
                        "lower_is_better",
                        "alert_threshold",
                        "cold",
                    ):
                        new_result[field] = test[field]

                    # Differentiate Raptor `pageload` tests from other `browsertime`
                    # tests while we compare and contrast.
                    new_result["type"] = "browsertime-%s" % test["type"]

                    # Try to get subtest values or use the defaults
                    # If values not available use the defaults
                    subtest_lower_is_better = test.get("subtest_lower_is_better", True)
                    new_result["subtest_lower_is_better"] = subtest_lower_is_better

                    new_result["subtest_unit"] = test.get("subtest_unit", "ms")
                    LOG.info("parsed new result: %s" % str(new_result))

                    # `extra_options` will also be populated with Gecko profiling flags in
                    # the future.
                    new_result["extra_options"] = []
                    if self.app != "firefox":
                        new_result["extra_options"].append(self.app)

                    return new_result

                if test["type"] == "pageload":
                    self.results.append(_new_pageload_result(new_result))
                elif test["type"] == "benchmark":
                    for i, item in enumerate(self.results):
                        if item["name"] == test["name"]:
                            # add page cycle custom measurements to the existing results
                            for measurement in new_result["measurements"].iteritems():
                                self.results[i]["measurements"][measurement[0]].extend(
                                    measurement[1]
                                )
                            break
                    else:
                        self.results.append(_new_benchmark_result(new_result))

        # now have all results gathered from all browsertime test URLs; format them for output
        output = BrowsertimeOutput(
            self.results, self.supporting_data, test_config["subtest_alert_on"]
        )
        output.set_browser_meta(self.browser_name, self.browser_version)
        output.summarize(test_names)
        success, out_perfdata = output.output(test_names)

        validate_success = True
        if not self.gecko_profile:
            validate_success = self._validate_treeherder_data(output, out_perfdata)

        if len(video_jobs) > 0:
            # The video list and application metadata (browser name and
            # optionally version) that will be used in the visual metrics task.
            jobs_json = {"jobs": video_jobs, "application": {"name": self.browser_name}}

            if self.browser_version is not None:
                jobs_json["application"]["version"] = self.browser_version

            jobs_file = os.path.join(self.result_dir(), "jobs.json")
            LOG.info(
                "Writing video jobs and application data {} into {}".format(
                    jobs_json, jobs_file
                )
            )
            with open(jobs_file, "w") as f:
                f.write(json.dumps(jobs_json))

        return success and validate_success


class MissingResultsError(Exception):
    pass
