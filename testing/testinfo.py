# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import errno
import json
import os
import posixpath
import re
import subprocess
from collections import defaultdict

import mozpack.path as mozpath
import requests
import six.moves.urllib_parse as urlparse
from mozbuild.base import MachCommandConditions as conditions
from mozbuild.base import MozbuildObject
from mozfile import which
from moztest.resolve import TestManifestLoader, TestResolver
from redo import retriable

REFERER = "https://wiki.developer.mozilla.org/en-US/docs/Mozilla/Test-Info"
MAX_DAYS = 30


class TestInfo(object):
    """
    Support 'mach test-info'.
    """

    def __init__(self, verbose):
        self.verbose = verbose
        here = os.path.abspath(os.path.dirname(__file__))
        self.build_obj = MozbuildObject.from_environment(cwd=here)

    def log_verbose(self, what):
        if self.verbose:
            print(what)


class TestInfoTests(TestInfo):
    """
    Support 'mach test-info tests': Detailed report of specified tests.
    """

    def __init__(self, verbose):
        TestInfo.__init__(self, verbose)

        self._hg = None
        if conditions.is_hg(self.build_obj):
            self._hg = which("hg")
            if not self._hg:
                raise OSError(errno.ENOENT, "Could not find 'hg' on PATH.")

        self._git = None
        if conditions.is_git(self.build_obj):
            self._git = which("git")
            if not self._git:
                raise OSError(errno.ENOENT, "Could not find 'git' on PATH.")

    def find_in_hg_or_git(self, test_name):
        if self._hg:
            cmd = [self._hg, "files", "-I", test_name]
        elif self._git:
            cmd = [self._git, "ls-files", test_name]
        else:
            return None
        try:
            out = subprocess.check_output(cmd, universal_newlines=True).splitlines()
        except subprocess.CalledProcessError:
            out = None
        return out

    def set_test_name(self):
        # Generating a unified report for a specific test is complicated
        # by differences in the test name used in various data sources.
        # Consider:
        #   - It is often convenient to request a report based only on
        #     a short file name, rather than the full path;
        #   - Bugs may be filed in bugzilla against a simple, short test
        #     name or the full path to the test;
        # This function attempts to find appropriate names for different
        # queries based on the specified test name.

        # full_test_name is full path to file in hg (or git)
        self.full_test_name = None
        out = self.find_in_hg_or_git(self.test_name)
        if out and len(out) == 1:
            self.full_test_name = out[0]
        elif out and len(out) > 1:
            print("Ambiguous test name specified. Found:")
            for line in out:
                print(line)
        else:
            out = self.find_in_hg_or_git("**/%s*" % self.test_name)
            if out and len(out) == 1:
                self.full_test_name = out[0]
            elif out and len(out) > 1:
                print("Ambiguous test name. Found:")
                for line in out:
                    print(line)
        if self.full_test_name:
            self.full_test_name.replace(os.sep, posixpath.sep)
            print("Found %s in source control." % self.full_test_name)
        else:
            print("Unable to validate test name '%s'!" % self.test_name)
            self.full_test_name = self.test_name

        # search for full_test_name in test manifests
        here = os.path.abspath(os.path.dirname(__file__))
        resolver = TestResolver.from_environment(
            cwd=here, loader_cls=TestManifestLoader
        )
        relpath = self.build_obj._wrap_path_argument(self.full_test_name).relpath()
        tests = list(resolver.resolve_tests(paths=[relpath]))
        if len(tests) == 1:
            relpath = self.build_obj._wrap_path_argument(tests[0]["manifest"]).relpath()
            print("%s found in manifest %s" % (self.full_test_name, relpath))
            if tests[0].get("flavor"):
                print("  flavor: %s" % tests[0]["flavor"])
            if tests[0].get("skip-if"):
                print("  skip-if: %s" % tests[0]["skip-if"])
            if tests[0].get("fail-if"):
                print("  fail-if: %s" % tests[0]["fail-if"])
        elif len(tests) == 0:
            print("%s not found in any test manifest!" % self.full_test_name)
        else:
            print("%s found in more than one manifest!" % self.full_test_name)

        # short_name is full_test_name without path
        self.short_name = None
        name_idx = self.full_test_name.rfind("/")
        if name_idx > 0:
            self.short_name = self.full_test_name[name_idx + 1 :]
        if self.short_name and self.short_name == self.test_name:
            self.short_name = None

    def get_platform(self, record):
        if "platform" in record["build"]:
            platform = record["build"]["platform"]
        else:
            platform = "-"
        platform_words = platform.split("-")
        types_label = ""
        # combine run and build types and eliminate duplicates
        run_types = []
        if "run" in record and "type" in record["run"]:
            run_types = record["run"]["type"]
            run_types = run_types if isinstance(run_types, list) else [run_types]
        build_types = []
        if "build" in record and "type" in record["build"]:
            build_types = record["build"]["type"]
            build_types = (
                build_types if isinstance(build_types, list) else [build_types]
            )
        run_types = list(set(run_types + build_types))
        # '1proc' is used as a treeherder label but does not appear in run types
        if "e10s" not in run_types:
            run_types = run_types + ["1proc"]
        for run_type in run_types:
            # chunked is not interesting
            if run_type == "chunked":
                continue
            # e10s is the default: implied
            if run_type == "e10s":
                continue
            # sometimes a build/run type is already present in the build platform
            if run_type in platform_words:
                continue
            if types_label:
                types_label += "-"
            types_label += run_type
        return "%s/%s:" % (platform, types_label)

    def report_bugs(self):
        # Report open bugs matching test name
        search = self.full_test_name
        if self.test_name:
            search = "%s,%s" % (search, self.test_name)
        if self.short_name:
            search = "%s,%s" % (search, self.short_name)
        payload = {"quicksearch": search, "include_fields": "id,summary"}
        response = requests.get("https://bugzilla.mozilla.org/rest/bug", payload)
        response.raise_for_status()
        json_response = response.json()
        print("\nBugzilla quick search for '%s':" % search)
        if "bugs" in json_response:
            for bug in json_response["bugs"]:
                print("Bug %s: %s" % (bug["id"], bug["summary"]))
        else:
            print("No bugs found.")

    def report(
        self,
        test_names,
        start,
        end,
        show_info,
        show_bugs,
    ):
        self.start = start
        self.end = end
        self.show_info = show_info

        if not self.show_info and not show_bugs:
            # by default, show everything
            self.show_info = True
            show_bugs = True

        for test_name in test_names:
            print("===== %s =====" % test_name)
            self.test_name = test_name
            if len(self.test_name) < 6:
                print("'%s' is too short for a test name!" % self.test_name)
                continue
            self.set_test_name()
            if show_bugs:
                self.report_bugs()


class TestInfoReport(TestInfo):
    """
    Support 'mach test-info report': Report of test runs summarized by
    manifest and component.
    """

    def __init__(self, verbose):
        TestInfo.__init__(self, verbose)
        self.threads = []

    @retriable(attempts=3, sleeptime=5, sleepscale=2)
    def get_url(self, target_url):
        # if we fail to get valid json (i.e. end point has malformed data), return {}
        retVal = {}
        try:
            self.log_verbose("getting url: %s" % target_url)
            r = requests.get(target_url, headers={"User-agent": "mach-test-info/1.0"})
            self.log_verbose("got status: %s" % r.status_code)
            r.raise_for_status()
            retVal = r.json()
        except json.decoder.JSONDecodeError:
            self.log_verbose("Error retrieving data from %s" % target_url)

        return retVal

    def update_report(self, by_component, result, path_mod):
        def update_item(item, label, value):
            # It is important to include any existing item value in case ActiveData
            # returns multiple records for the same test; that can happen if the report
            # sometimes maps more than one ActiveData record to the same path.
            new_value = item.get(label, 0) + value
            if type(new_value) == int:
                item[label] = new_value
            else:
                item[label] = float(round(new_value, 2))  # pylint: disable=W1633

        if "test" in result and "tests" in by_component:
            test = result["test"]
            if path_mod:
                test = path_mod(test)
            for bc in by_component["tests"]:
                for item in by_component["tests"][bc]:
                    if test == item["test"]:
                        # pylint: disable=W1633
                        seconds = float(round(result.get("duration", 0), 2))
                        update_item(item, "total run time, seconds", seconds)
                        update_item(item, "total runs", result.get("count", 0))
                        update_item(item, "skipped runs", result.get("skips", 0))
                        update_item(item, "failed runs", result.get("failures", 0))
                        return True
        return False

    def path_mod_reftest(self, path):
        # "<path1> == <path2>" -> "<path1>"
        path = path.split(" ")[0]
        # "<path>?<params>" -> "<path>"
        path = path.split("?")[0]
        # "<path>#<fragment>" -> "<path>"
        path = path.split("#")[0]
        return path

    def path_mod_jsreftest(self, path):
        # "<path>;assert" -> "<path>"
        path = path.split(";")[0]
        return path

    def path_mod_marionette(self, path):
        # "<path> <test-name>" -> "<path>"
        path = path.split(" ")[0]
        # "part1\part2" -> "part1/part2"
        path = path.replace("\\", os.path.sep)
        return path

    def path_mod_wpt(self, path):
        if path[0] == os.path.sep:
            # "/<path>" -> "<path>"
            path = path[1:]
        # "<path>" -> "testing/web-platform/tests/<path>"
        path = os.path.join("testing", "web-platform", "tests", path)
        # "<path>?<params>" -> "<path>"
        path = path.split("?")[0]
        return path

    def path_mod_jittest(self, path):
        # "part1\part2" -> "part1/part2"
        path = path.replace("\\", os.path.sep)
        # "<path>" -> "js/src/jit-test/tests/<path>"
        return os.path.join("js", "src", "jit-test", "tests", path)

    def path_mod_xpcshell(self, path):
        # <manifest>.{ini|toml}:<path> -> "<path>"
        path = path.split(":")[-1]
        return path

    def description(
        self,
        components,
        flavor,
        subsuite,
        paths,
        show_manifests,
        show_tests,
        show_summary,
        show_annotations,
        filter_values,
        filter_keys,
        start_date,
        end_date,
    ):
        # provide a natural language description of the report options
        what = []
        if show_manifests:
            what.append("test manifests")
        if show_tests:
            what.append("tests")
        if show_annotations:
            what.append("test manifest annotations")
        if show_summary and len(what) == 0:
            what.append("summary of tests only")
        if len(what) > 1:
            what[-1] = "and " + what[-1]
        what = ", ".join(what)
        d = "Test summary report for " + what
        if components:
            d += ", in specified components (%s)" % components
        else:
            d += ", in all components"
        if flavor:
            d += ", in specified flavor (%s)" % flavor
        if subsuite:
            d += ", in specified subsuite (%s)" % subsuite
        if paths:
            d += ", on specified paths (%s)" % paths
        if filter_values:
            d += ", containing '%s'" % filter_values
            if filter_keys:
                d += " in manifest keys '%s'" % filter_keys
            else:
                d += " in any part of manifest entry"
        d += ", including historical run-time data for the last "

        start = datetime.datetime.strptime(start_date, "%Y-%m-%d")
        end = datetime.datetime.strptime(end_date, "%Y-%m-%d")
        d += "%s days on trunk (autoland/m-c)" % ((end - start).days)
        d += " as of %s." % end_date
        return d

    # TODO: this is hacked for now and very limited
    def parse_test(self, summary):
        if summary.endswith("single tracking bug"):
            name_part = summary.split("|")[0]  # remove 'single tracking bug'
            name_part.strip()
            return name_part.split()[-1]  # get just the test name, not extra words
        return None

    def get_runcount_data(self, runcounts_input_file, start, end):
        # TODO: use start/end properly
        if runcounts_input_file:
            try:
                with open(runcounts_input_file, "r") as f:
                    runcounts = json.load(f)
            except:
                print("Unable to load runcounts from path: %s" % runcounts_input_file)
                raise
        else:
            runcounts = self.get_runcounts(days=MAX_DAYS)
        runcounts = self.squash_runcounts(runcounts, days=MAX_DAYS)
        return runcounts

    def get_testinfoall_index_url(self):
        import taskcluster

        index = taskcluster.Index(
            {
                "rootUrl": "https://firefox-ci-tc.services.mozilla.com",
            }
        )
        route = "gecko.v2.mozilla-central.latest.source.test-info-all"
        queue = taskcluster.Queue(
            {
                "rootUrl": "https://firefox-ci-tc.services.mozilla.com",
            }
        )

        task_id = index.findTask(route)["taskId"]
        artifacts = queue.listLatestArtifacts(task_id)["artifacts"]

        url = ""
        for artifact in artifacts:
            if artifact["name"].endswith("test-run-info.json"):
                url = queue.buildUrl("getLatestArtifact", task_id, artifact["name"])
                break
        return url

    def get_runcounts(self, days=MAX_DAYS):
        testrundata = {}
        # get historical data from test-info job artifact; if missing get fresh
        url = self.get_testinfoall_index_url()
        print("INFO: requesting runcounts url: %s" % url)
        olddata = self.get_url(url)

        # fill in any holes we have
        endday = datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(
            days=1
        )
        startday = endday - datetime.timedelta(days=days)
        urls_to_fetch = []
        # build list of dates with missing data
        while startday < endday:
            nextday = startday + datetime.timedelta(days=1)
            if not olddata.get(str(nextday.date()), {}):
                url = "https://treeherder.mozilla.org/api/groupsummary/"
                url += "?startdate=%s&enddate=%s" % (
                    startday.date(),
                    nextday.date(),
                )
                urls_to_fetch.append([str(nextday.date()), url])
            testrundata[str(nextday.date())] = olddata.get(str(nextday.date()), {})

            startday = nextday

        # limit missing data collection to 5 most recent days days to reduce overall runtime
        for date, url in urls_to_fetch[-5:]:
            try:
                testrundata[date] = self.get_url(url)
            except requests.exceptions.HTTPError:
                # We want to see other errors, but can accept HTTPError failures
                print(f"Unable to retrieve results for url: {url}")
                pass

        return testrundata

    def squash_runcounts(self, runcounts, days=MAX_DAYS):
        # squash all testrundata together into 1 big happy family for the last X days
        endday = datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(
            days=1
        )
        oldest = endday - datetime.timedelta(days=days)

        testgroup_runinfo = defaultdict(lambda: defaultdict(int))

        retVal = {}
        for datekey in runcounts.keys():
            # strip out older days
            if datetime.date.fromisoformat(datekey) < oldest.date():
                continue

            jtn = runcounts[datekey].get("job_type_names", {})
            if not jtn:
                print("Warning: Missing job type names from date: %s" % datekey)
                continue

            for m in runcounts[datekey]["manifests"]:
                man_name = list(m.keys())[0]

                for job_type_id, result, classification, count in m[man_name]:
                    # format: job_type_name, result, classification, count
                    # find matching jtn, result, classification and increment 'count'
                    job_name = jtn[job_type_id]
                    key = (job_name, result, classification)
                    testgroup_runinfo[man_name][key] += count

        for m in testgroup_runinfo:
            retVal[m] = [
                list(x) + [testgroup_runinfo[m][x]] for x in testgroup_runinfo[m]
            ]
        return retVal

    def get_intermittent_failure_data(self, start, end):
        retVal = {}

        # get IFV bug list
        # i.e. https://th.m.o/api/failures/?startday=2022-06-22&endday=2022-06-29&tree=all
        url = (
            "https://treeherder.mozilla.org/api/failures/?startday=%s&endday=%s&tree=trunk"
            % (start, end)
        )
        if_data = self.get_url(url)
        buglist = [x["bug_id"] for x in if_data]

        # get bug data for summary, 800 bugs at a time
        # i.e. https://b.m.o/rest/bug?include_fields=id,product,component,summary&id=1,2,3...
        max_bugs = 800
        bug_data = []
        fields = ["id", "product", "component", "summary"]
        for bug_index in range(0, len(buglist), max_bugs):
            bugs = [str(x) for x in buglist[bug_index : bug_index + max_bugs]]
            if not bugs:
                print(f"warning: found no bugs in range {bug_index}, +{max_bugs}")
                continue

            url = "https://bugzilla.mozilla.org/rest/bug?include_fields=%s&id=%s" % (
                ",".join(fields),
                ",".join(bugs),
            )
            data = self.get_url(url)
            if data and "bugs" in data.keys():
                bug_data.extend(data["bugs"])

        # for each summary, parse filename, store component
        # IF we find >1 bug with same testname, for now summarize as one
        for bug in bug_data:
            test_name = self.parse_test(bug["summary"])
            if not test_name:
                continue

            c = int([x["bug_count"] for x in if_data if x["bug_id"] == bug["id"]][0])
            if test_name not in retVal.keys():
                retVal[test_name] = {
                    "id": bug["id"],
                    "count": 0,
                    "product": bug["product"],
                    "component": bug["component"],
                }
            retVal[test_name]["count"] += c

            if bug["product"] != retVal[test_name]["product"]:
                print(
                    "ERROR | %s | mismatched bugzilla product, bugzilla (%s) != repo (%s)"
                    % (bug["id"], bug["product"], retVal[test_name]["product"])
                )
            if bug["component"] != retVal[test_name]["component"]:
                print(
                    "ERROR | %s | mismatched bugzilla component, bugzilla (%s) != repo (%s)"
                    % (bug["id"], bug["component"], retVal[test_name]["component"])
                )
        return retVal

    def report(
        self,
        components,
        flavor,
        subsuite,
        paths,
        show_manifests,
        show_tests,
        show_summary,
        show_annotations,
        filter_values,
        filter_keys,
        show_components,
        output_file,
        start,
        end,
        show_testruns,
        runcounts_input_file,
    ):
        def matches_filters(test):
            """
            Return True if all of the requested filter_values are found in this test;
            if filter_keys are specified, restrict search to those test keys.
            """
            for value in filter_values:
                value_found = False
                for key in test:
                    if not filter_keys or key in filter_keys:
                        if re.search(value, test[key]):
                            value_found = True
                            break
                if not value_found:
                    return False
            return True

        start_time = datetime.datetime.now()

        # Ensure useful report by default
        if (
            not show_manifests
            and not show_tests
            and not show_summary
            and not show_annotations
        ):
            show_manifests = True
            show_summary = True

        by_component = {}
        if components:
            components = components.split(",")
        if filter_keys:
            filter_keys = filter_keys.split(",")
        if filter_values:
            filter_values = filter_values.split(",")
        else:
            filter_values = []
        display_keys = (filter_keys or []) + ["skip-if", "fail-if", "fails-if"]
        display_keys = set(display_keys)
        ifd = self.get_intermittent_failure_data(start, end)

        runcount = {}
        if show_testruns and os.environ.get("GECKO_HEAD_REPOSITORY", "") in [
            "https://hg.mozilla.org/mozilla-central",
            "https://hg.mozilla.org/try",
        ]:
            runcount = self.get_runcount_data(runcounts_input_file, start, end)

        print("Finding tests...")
        here = os.path.abspath(os.path.dirname(__file__))
        resolver = TestResolver.from_environment(
            cwd=here, loader_cls=TestManifestLoader
        )
        tests = list(
            resolver.resolve_tests(paths=paths, flavor=flavor, subsuite=subsuite)
        )

        manifest_paths = set()
        for t in tests:
            if t.get("manifest", None):
                manifest_path = t["manifest"]
                if t.get("ancestor_manifest", None):
                    manifest_path = "%s:%s" % (t["ancestor_manifest"], t["manifest"])
                manifest_paths.add(manifest_path)
        manifest_count = len(manifest_paths)
        print(
            "Resolver found {} tests, {} manifests".format(len(tests), manifest_count)
        )

        if show_manifests:
            topsrcdir = self.build_obj.topsrcdir
            by_component["manifests"] = {}
            manifest_paths = list(manifest_paths)
            manifest_paths.sort()
            relpaths = []
            for manifest_path in manifest_paths:
                relpath = mozpath.relpath(manifest_path, topsrcdir)
                if mozpath.commonprefix((manifest_path, topsrcdir)) != topsrcdir:
                    continue
                relpaths.append(relpath)
            reader = self.build_obj.mozbuild_reader(config_mode="empty")
            files_info = reader.files_info(relpaths)
            for manifest_path in manifest_paths:
                relpath = mozpath.relpath(manifest_path, topsrcdir)
                if mozpath.commonprefix((manifest_path, topsrcdir)) != topsrcdir:
                    continue
                manifest_info = None
                if relpath in files_info:
                    bug_component = files_info[relpath].get("BUG_COMPONENT")
                    if bug_component:
                        key = "{}::{}".format(
                            bug_component.product, bug_component.component
                        )
                    else:
                        key = "<unknown bug component>"
                    if (not components) or (key in components):
                        manifest_info = {"manifest": relpath, "tests": 0, "skipped": 0}
                        rkey = key if show_components else "all"
                        if rkey in by_component["manifests"]:
                            by_component["manifests"][rkey].append(manifest_info)
                        else:
                            by_component["manifests"][rkey] = [manifest_info]
                if manifest_info:
                    for t in tests:
                        if t["manifest"] == manifest_path:
                            manifest_info["tests"] += 1
                            if t.get("skip-if"):
                                manifest_info["skipped"] += 1
            for key in by_component["manifests"]:
                by_component["manifests"][key].sort(key=lambda k: k["manifest"])

        if show_tests:
            by_component["tests"] = {}

        if show_tests or show_summary or show_annotations:
            test_count = 0
            failed_count = 0
            skipped_count = 0
            annotation_count = 0
            condition_count = 0
            component_set = set()
            relpaths = []
            conditions = {}
            known_unconditional_annotations = ["skip", "fail", "asserts", "random"]
            known_conditional_annotations = [
                "skip-if",
                "fail-if",
                "run-if",
                "fails-if",
                "fuzzy-if",
                "random-if",
                "asserts-if",
            ]
            for t in tests:
                relpath = t.get("srcdir_relpath")
                relpaths.append(relpath)
            reader = self.build_obj.mozbuild_reader(config_mode="empty")
            files_info = reader.files_info(relpaths)
            for t in tests:
                if not matches_filters(t):
                    continue
                if "referenced-test" in t:
                    # Avoid double-counting reftests: disregard reference file entries
                    continue
                if show_annotations:
                    for key in t:
                        if key in known_unconditional_annotations:
                            annotation_count += 1
                        if key in known_conditional_annotations:
                            annotation_count += 1
                            # Here 'key' is a manifest annotation type like 'skip-if' and t[key]
                            # is the associated condition. For example, the manifestparser
                            # manifest annotation, "skip-if = os == 'win'", is expected to be
                            # encoded as t['skip-if'] = "os == 'win'".
                            # To allow for reftest manifests, t[key] may have multiple entries
                            # separated by ';', each corresponding to a condition for that test
                            # and annotation type. For example,
                            # "skip-if(Android&&webrender) skip-if(OSX)", would be
                            # encoded as t['skip-if'] = "Android&&webrender;OSX".
                            annotation_conditions = t[key].split(";")

                            # if key has \n in it, we need to strip it. for manifestparser format
                            #  1) from the beginning of the line
                            #  2) different conditions if in the middle of the line
                            annotation_conditions = [
                                x.strip("\n") for x in annotation_conditions
                            ]
                            temp = []
                            for condition in annotation_conditions:
                                temp.extend(condition.split("\n"))
                            annotation_conditions = temp

                            for condition in annotation_conditions:
                                condition_count += 1
                                # Trim reftest fuzzy-if ranges: everything after the first comma
                                # eg. "Android,0-2,1-3" -> "Android"
                                condition = condition.split(",")[0]
                                if condition not in conditions:
                                    conditions[condition] = 0
                                conditions[condition] += 1
                test_count += 1
                relpath = t.get("srcdir_relpath")
                if relpath in files_info:
                    bug_component = files_info[relpath].get("BUG_COMPONENT")
                    if bug_component:
                        key = "{}::{}".format(
                            bug_component.product, bug_component.component
                        )
                    else:
                        key = "<unknown bug component>"
                    if (not components) or (key in components):
                        component_set.add(key)
                        test_info = {"test": relpath}
                        for test_key in display_keys:
                            value = t.get(test_key)
                            if value:
                                test_info[test_key] = value
                        if t.get("fail-if"):
                            failed_count += 1
                        if t.get("fails-if"):
                            failed_count += 1
                        if t.get("skip-if"):
                            skipped_count += 1

                        if "manifest_relpath" in t and "manifest" in t:
                            if "web-platform" in t["manifest_relpath"]:
                                test_info["manifest"] = [t["manifest"]]
                            else:
                                test_info["manifest"] = [t["manifest_relpath"]]

                            # handle included manifests as ancestor:child
                            if t.get("ancestor_manifest", None):
                                test_info["manifest"] = [
                                    "%s:%s"
                                    % (t["ancestor_manifest"], test_info["manifest"][0])
                                ]

                        # add in intermittent failure data
                        if ifd.get(relpath):
                            if_data = ifd.get(relpath)
                            test_info["failure_count"] = if_data["count"]
                            if show_testruns:
                                total_runs = 0
                                for m in test_info["manifest"]:
                                    if m in runcount.keys():
                                        for x in runcount.get(m, []):
                                            if not x:
                                                break
                                            total_runs += x[3]
                                if total_runs > 0:
                                    test_info["total_runs"] = total_runs

                        if show_tests:
                            rkey = key if show_components else "all"
                            if rkey in by_component["tests"]:
                                # Avoid duplicates: Some test paths have multiple TestResolver
                                # entries, as when a test is included by multiple manifests.
                                found = False
                                for ctest in by_component["tests"][rkey]:
                                    if ctest["test"] == test_info["test"]:
                                        found = True
                                        break
                                if not found:
                                    by_component["tests"][rkey].append(test_info)
                                else:
                                    for ti in by_component["tests"][rkey]:
                                        if ti["test"] == test_info["test"]:
                                            if (
                                                test_info["manifest"][0]
                                                not in ti["manifest"]
                                            ):
                                                ti_manifest = test_info["manifest"]
                                                if test_info.get(
                                                    "ancestor_manifest", None
                                                ):
                                                    ti_manifest = "%s:%s" % (
                                                        test_info["ancestor_manifest"],
                                                        ti_manifest,
                                                    )
                                                ti["manifest"].extend(ti_manifest)
                            else:
                                by_component["tests"][rkey] = [test_info]
            if show_tests:
                for key in by_component["tests"]:
                    by_component["tests"][key].sort(key=lambda k: k["test"])

        by_component["description"] = self.description(
            components,
            flavor,
            subsuite,
            paths,
            show_manifests,
            show_tests,
            show_summary,
            show_annotations,
            filter_values,
            filter_keys,
            start,
            end,
        )

        if show_summary:
            by_component["summary"] = {}
            by_component["summary"]["components"] = len(component_set)
            by_component["summary"]["manifests"] = manifest_count
            by_component["summary"]["tests"] = test_count
            by_component["summary"]["failed tests"] = failed_count
            by_component["summary"]["skipped tests"] = skipped_count

        if show_annotations:
            by_component["annotations"] = {}
            by_component["annotations"]["total annotations"] = annotation_count
            by_component["annotations"]["total conditions"] = condition_count
            by_component["annotations"]["unique conditions"] = len(conditions)
            by_component["annotations"]["conditions"] = conditions

        self.write_report(by_component, output_file)

        end_time = datetime.datetime.now()
        self.log_verbose(
            "%d seconds total to generate report"
            % (end_time - start_time).total_seconds()
        )

    def write_report(self, by_component, output_file):
        json_report = json.dumps(by_component, indent=2, sort_keys=True)
        if output_file:
            output_file = os.path.abspath(output_file)
            output_dir = os.path.dirname(output_file)
            if not os.path.isdir(output_dir):
                os.makedirs(output_dir)

            with open(output_file, "w") as f:
                f.write(json_report)
        else:
            print(json_report)

    def report_diff(self, before, after, output_file):
        """
        Support for 'mach test-info report-diff'.
        """

        def get_file(path_or_url):
            if urlparse.urlparse(path_or_url).scheme:
                response = requests.get(path_or_url)
                response.raise_for_status()
                return json.loads(response.text)
            with open(path_or_url) as f:
                return json.load(f)

        report1 = get_file(before)
        report2 = get_file(after)

        by_component = {"tests": {}, "summary": {}}
        self.diff_summaries(by_component, report1["summary"], report2["summary"])
        self.diff_all_components(by_component, report1["tests"], report2["tests"])
        self.write_report(by_component, output_file)

    def diff_summaries(self, by_component, summary1, summary2):
        """
        Update by_component with comparison of summaries.
        """
        all_keys = set(summary1.keys()) | set(summary2.keys())
        for key in all_keys:
            delta = summary2.get(key, 0) - summary1.get(key, 0)
            by_component["summary"]["%s delta" % key] = delta

    def diff_all_components(self, by_component, tests1, tests2):
        """
        Update by_component with any added/deleted tests, for all components.
        """
        self.added_count = 0
        self.deleted_count = 0
        for component in tests1:
            component1 = tests1[component]
            component2 = [] if component not in tests2 else tests2[component]
            self.diff_component(by_component, component, component1, component2)
        for component in tests2:
            if component not in tests1:
                component2 = tests2[component]
                self.diff_component(by_component, component, [], component2)
        by_component["summary"]["added tests"] = self.added_count
        by_component["summary"]["deleted tests"] = self.deleted_count

    def diff_component(self, by_component, component, component1, component2):
        """
        Update by_component[component] with any added/deleted tests for the
        named component.
        "added": tests found in component2 but missing from component1.
        "deleted": tests found in component1 but missing from component2.
        """
        tests1 = set([t["test"] for t in component1])
        tests2 = set([t["test"] for t in component2])
        deleted = tests1 - tests2
        added = tests2 - tests1
        if deleted or added:
            by_component["tests"][component] = {}
            if deleted:
                by_component["tests"][component]["deleted"] = sorted(list(deleted))
            if added:
                by_component["tests"][component]["added"] = sorted(list(added))
        self.added_count += len(added)
        self.deleted_count += len(deleted)
        common = len(tests1.intersection(tests2))
        self.log_verbose(
            "%s: %d deleted, %d added, %d common"
            % (component, len(deleted), len(added), common)
        )
