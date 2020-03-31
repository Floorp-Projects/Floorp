# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import datetime
import errno
import json
import os
import posixpath
import re
import requests
import six.moves.urllib_parse as urlparse
import subprocess
import threading
import traceback
import mozpack.path as mozpath
from moztest.resolve import TestResolver, TestManifestLoader
from mozfile import which

from mozbuild.base import MozbuildObject, MachCommandConditions as conditions

ACTIVEDATA_RECORD_LIMIT = 10000
MAX_ACTIVEDATA_CONCURRENCY = 5
MAX_ACTIVEDATA_RETRIES = 5
REFERER = 'https://wiki.developer.mozilla.org/en-US/docs/Mozilla/Test-Info'


class TestInfo(object):
    """
    Support 'mach test-info'.
    """
    def __init__(self, verbose):
        self.verbose = verbose
        here = os.path.abspath(os.path.dirname(__file__))
        self.build_obj = MozbuildObject.from_environment(cwd=here)
        self.total_activedata_seconds = 0

    def log_verbose(self, what):
        if self.verbose:
            print(what)

    def activedata_query(self, query):
        start_time = datetime.datetime.now()
        self.log_verbose(start_time)
        self.log_verbose(json.dumps(query))
        response = requests.post("http://activedata.allizom.org/query",
                                 data=json.dumps(query),
                                 headers={'referer': REFERER},
                                 stream=True)
        end_time = datetime.datetime.now()
        self.total_activedata_seconds += (end_time - start_time).total_seconds()
        self.log_verbose(end_time)
        self.log_verbose(response)
        response.raise_for_status()
        data = response.json()["data"]
        self.log_verbose("response length: %d" % len(data))
        return data


class ActiveDataThread(threading.Thread):
    """
    A thread to query ActiveData and wait for its response.
    """
    def __init__(self, name, ti, query, context):
        threading.Thread.__init__(self, name=name)
        self.ti = ti
        self.query = query
        self.context = context
        self.response = None

    def run(self):
        attempt = 1
        while attempt < MAX_ACTIVEDATA_RETRIES and not self.response:
            try:
                self.response = self.ti.activedata_query(self.query)
                if not self.response:
                    self.ti.log_verbose("%s: no data received for query" % self.name)
                    self.response = []
                    break
            except Exception:
                self.ti.log_verbose("%s: Exception on attempt #%d:" % (self.name, attempt))
                traceback.print_exc()
                attempt += 1


class TestInfoTests(TestInfo):
    """
    Support 'mach test-info tests': Detailed report of specified tests.
    """
    def __init__(self, verbose):
        TestInfo.__init__(self, verbose)

        self._hg = None
        if conditions.is_hg(self.build_obj):
            self._hg = which('hg')
            if not self._hg:
                raise OSError(errno.ENOENT, "Could not find 'hg' on PATH.")

        self._git = None
        if conditions.is_git(self.build_obj):
            self._git = which('git')
            if not self._git:
                raise OSError(errno.ENOENT, "Could not find 'git' on PATH.")

    def find_in_hg_or_git(self, test_name):
        if self._hg:
            cmd = [self._hg, 'files', '-I', test_name]
        elif self._git:
            cmd = [self._git, 'ls-files', test_name]
        else:
            return None
        try:
            out = subprocess.check_output(cmd).splitlines()
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
        #   - In ActiveData, the full path is usually used, but sometimes
        #     also includes additional path components outside of the
        #     mercurial repo (common for reftests).
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
            out = self.find_in_hg_or_git('**/%s*' % self.test_name)
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
        resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)
        relpath = self.build_obj._wrap_path_argument(self.full_test_name).relpath()
        tests = list(resolver.resolve_tests(paths=[relpath]))
        if len(tests) == 1:
            relpath = self.build_obj._wrap_path_argument(tests[0]['manifest']).relpath()
            print("%s found in manifest %s" % (self.full_test_name, relpath))
            if tests[0].get('flavor'):
                print("  flavor: %s" % tests[0]['flavor'])
            if tests[0].get('skip-if'):
                print("  skip-if: %s" % tests[0]['skip-if'])
            if tests[0].get('fail-if'):
                print("  fail-if: %s" % tests[0]['fail-if'])
        elif len(tests) == 0:
            print("%s not found in any test manifest!" % self.full_test_name)
        else:
            print("%s found in more than one manifest!" % self.full_test_name)

        # short_name is full_test_name without path
        self.short_name = None
        name_idx = self.full_test_name.rfind('/')
        if name_idx > 0:
            self.short_name = self.full_test_name[name_idx + 1:]
        if self.short_name and self.short_name == self.test_name:
            self.short_name = None

        if not (self.show_results or self.show_durations or self.show_tasks):
            # no need to determine ActiveData name if not querying
            return

    def set_activedata_test_name(self):
        # activedata_test_name is name in ActiveData
        self.activedata_test_name = None
        simple_names = [
            self.full_test_name,
            self.test_name,
            self.short_name
        ]
        simple_names = [x for x in simple_names if x]
        searches = [
            {"in": {"result.test": simple_names}},
        ]
        regex_names = [".*%s.*" % re.escape(x) for x in simple_names if x]
        for r in regex_names:
            searches.append({"regexp": {"result.test": r}})
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 10,
            "groupby": ["result.test"],
            "where": {"and": [
                {"or": searches},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        print("Querying ActiveData...")  # Following query can take a long time
        data = self.activedata_query(query)
        if data and len(data) > 0:
            self.activedata_test_name = [
                d['result']['test']
                for p in simple_names + regex_names
                for d in data
                if re.match(p + "$", d['result']['test'])
            ][0]  # first match is best match
        if self.activedata_test_name:
            print("Found records matching '%s' in ActiveData." %
                  self.activedata_test_name)
        else:
            print("Unable to find matching records in ActiveData; using %s!" %
                  self.test_name)
            self.activedata_test_name = self.test_name

    def get_run_types(self, record):
        types_label = ""
        if 'run' in record and 'type' in record['run']:
            run_types = record['run']['type']
            run_types = run_types if isinstance(run_types, list) else [run_types]
            fission = True if 'fis' in run_types else False
            for run_type in run_types:
                # chunked is not interesting
                if run_type == 'chunked':
                    continue
                # fission implies e10s
                if fission and run_type == 'e10s':
                    continue
                types_label += "-" + run_type
        return types_label

    def get_platform(self, record):
        if 'platform' in record['build']:
            platform = record['build']['platform']
        else:
            platform = "-"
        tp = record['build']['type']
        if type(tp) is list:
            tp = "-".join(tp)
        return "%s/%s%s:" % (platform, tp, self.get_run_types(record))

    def report_test_results(self):
        # Report test pass/fail summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 100,
            "groupby": ["build.platform", "build.type", "run.type"],
            "select": [
                {"aggregate": "count"},
                {
                    "name": "failures",
                    "value": {"case": [
                        {"when": {"eq": {"result.ok": "F"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                },
                {
                    "name": "skips",
                    "value": {"case": [
                        {"when": {"eq": {"result.status": "SKIP"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                }
            ],
            "where": {"and": [
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        print("\nTest results for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        data = self.activedata_query(query)
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            worst_rate = 0.0
            worst_platform = None
            total_runs = 0
            total_failures = 0
            for record in data:
                platform = self.get_platform(record)
                if platform.startswith("-"):
                    continue
                runs = record['count']
                total_runs = total_runs + runs
                failures = record.get('failures', 0)
                skips = record.get('skips', 0)
                total_failures = total_failures + failures
                rate = (float)(failures) / runs
                if rate >= worst_rate:
                    worst_rate = rate
                    worst_platform = platform
                    worst_failures = failures
                    worst_runs = runs
                print("%-40s %6d failures (%6d skipped) in %6d runs" % (
                    platform, failures, skips, runs))
            print("\nTotal: %d failures in %d runs or %.3f failures/run" %
                  (total_failures, total_runs, (float)(total_failures) / total_runs))
            if worst_failures > 0:
                print("Worst rate on %s %d failures in %d runs or %.3f failures/run" %
                      (worst_platform, worst_failures, worst_runs, worst_rate))
        else:
            print("No test result data found.")

    def report_test_durations(self):
        # Report test durations summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 100,
            "groupby": ["build.platform", "build.type", "run.type"],
            "select": [
                {"value": "result.duration",
                    "aggregate": "average", "name": "average"},
                {"value": "result.duration", "aggregate": "min", "name": "min"},
                {"value": "result.duration", "aggregate": "max", "name": "max"},
                {"aggregate": "count"}
            ],
            "where": {"and": [
                {"eq": {"result.ok": "T"}},
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        data = self.activedata_query(query)
        print("\nTest durations for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            for record in data:
                platform = self.get_platform(record)
                if platform.startswith("-"):
                    continue
                print("%-40s %6.2f s (%.2f s - %.2f s over %d runs)" % (
                    platform, record['average'], record['min'],
                    record['max'], record['count']))
        else:
            print("No test durations found.")

    def report_test_tasks(self):
        # Report test tasks summary from ActiveData
        query = {
            "from": "unittest",
            "format": "list",
            "limit": 1000,
            "select": ["build.platform", "build.type", "run.type", "run.name"],
            "where": {"and": [
                {"eq": {"result.test": self.activedata_test_name}},
                {"in": {"build.branch": self.branches.split(',')}},
                {"gt": {"run.timestamp": {"date": self.start}}},
                {"lt": {"run.timestamp": {"date": self.end}}}
            ]}
        }
        data = self.activedata_query(query)
        print("\nTest tasks for %s on %s between %s and %s" %
              (self.activedata_test_name, self.branches, self.start, self.end))
        if data and len(data) > 0:
            data.sort(key=self.get_platform)
            consolidated = {}
            for record in data:
                platform = self.get_platform(record)
                if platform not in consolidated:
                    consolidated[platform] = {}
                if record['run']['name'] in consolidated[platform]:
                    consolidated[platform][record['run']['name']] += 1
                else:
                    consolidated[platform][record['run']['name']] = 1
            for key in sorted(consolidated.keys()):
                tasks = ""
                for task in consolidated[key].keys():
                    if tasks:
                        tasks += "\n%-40s " % ""
                    tasks += task
                    tasks += " in %d runs" % consolidated[key][task]
                print("%-40s %s" % (key, tasks))
        else:
            print("No test tasks found.")

    def report_bugs(self):
        # Report open bugs matching test name
        search = self.full_test_name
        if self.test_name:
            search = '%s,%s' % (search, self.test_name)
        if self.short_name:
            search = '%s,%s' % (search, self.short_name)
        payload = {'quicksearch': search,
                   'include_fields': 'id,summary'}
        response = requests.get('https://bugzilla.mozilla.org/rest/bug',
                                payload)
        response.raise_for_status()
        json_response = response.json()
        print("\nBugzilla quick search for '%s':" % search)
        if 'bugs' in json_response:
            for bug in json_response['bugs']:
                print("Bug %s: %s" % (bug['id'], bug['summary']))
        else:
            print("No bugs found.")

    def report(self, test_names, branches, start, end,
               show_info, show_results, show_durations, show_tasks, show_bugs):
        self.branches = branches
        self.start = start
        self.end = end
        self.show_info = show_info
        self.show_results = show_results
        self.show_durations = show_durations
        self.show_tasks = show_tasks

        if (not self.show_info and
            not self.show_results and
            not self.show_durations and
            not self.show_tasks and
                not show_bugs):
            # by default, show everything
            self.show_info = True
            self.show_results = True
            self.show_durations = True
            self.show_tasks = True
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
            self.set_activedata_test_name()
            if self.show_results:
                self.report_test_results()
            if self.show_durations:
                self.report_test_durations()
            if self.show_tasks:
                self.report_test_tasks()


class TestInfoLongRunningTasks(TestInfo):
    """
    Support 'mach test-info long-tasks': Summary of tasks approaching their max-run-time.
    """
    def __init__(self, verbose):
        TestInfo.__init__(self, verbose)

    def report(self, branches, start, end, threshold_pct, filter_threshold_pct):

        def get_long_running_ratio(record):
            count = record['count']
            tasks_gt_pct = record['tasks_gt_pct']
            return count / tasks_gt_pct

        # Search test durations in ActiveData for long-running tests
        query = {
            "from": "task",
            "format": "list",
            "groupby": ["run.name"],
            "limit": 1000,
            "select": [
                {
                    "value": "task.maxRunTime",
                    "aggregate": "median",
                    "name": "max_run_time"
                },
                {
                    "aggregate": "count"
                },
                {
                    "value": {
                        "when": {
                            "gt": [
                                {
                                    "div": ["action.duration", "task.maxRunTime"]
                                }, threshold_pct/100.0
                            ]
                        },
                        "then": 1
                    },
                    "aggregate": "sum",
                    "name": "tasks_gt_pct"
                },
            ],
            "where": {"and": [
                {"in": {"build.branch": branches.split(',')}},
                {"gt": {"task.run.start_time": {"date": start}}},
                {"lte": {"task.run.start_time": {"date": end}}},
                {"eq": {"task.state": "completed"}},
            ]}
        }
        data = self.activedata_query(query)
        print("\nTasks nearing their max-run-time on %s between %s and %s" %
              (branches, start, end))
        if data and len(data) > 0:
            filtered = []
            for record in data:
                if 'tasks_gt_pct' in record:
                    count = record['count']
                    tasks_gt_pct = record['tasks_gt_pct']
                    if float(tasks_gt_pct) / count > filter_threshold_pct / 100.0:
                        filtered.append(record)
            filtered.sort(key=get_long_running_ratio)
            if not filtered:
                print("No long running tasks found.")
            for record in filtered:
                name = record['run']['name']
                count = record['count']
                max_run_time = record['max_run_time']
                tasks_gt_pct = record['tasks_gt_pct']
                print("%-55s: %d of %d runs (%.1f%%) exceeded %d%% of max-run-time (%d s)" %
                      (name, tasks_gt_pct, count, tasks_gt_pct * 100 / count,
                       threshold_pct, max_run_time))
        else:
            print("No tasks found.")


class TestInfoReport(TestInfo):
    """
    Support 'mach test-info report': Report of test runs summarized by
    manifest and component.
    """
    def __init__(self, verbose):
        TestInfo.__init__(self, verbose)
        self.total_activedata_matches = 0
        self.threads = []

    def add_activedata_for_suite(self, label, branches, days,
                                 suite_clause, tests_clause, path_mod):
        dates_clause = {"date": "today-%dday" % days}
        where_conditions = [
            suite_clause,
            {"in": {"repo.branch.name": branches.split(',')}},
            {"gt": {"run.timestamp": dates_clause}},
        ]
        if tests_clause:
            where_conditions.append(tests_clause)
        ad_query = {
            "from": "unittest",
            "limit": ACTIVEDATA_RECORD_LIMIT,
            "format": "list",
            "groupby": ["result.test"],
            "select": [
                {
                    "name": "result.count",
                    "aggregate": "count"
                },
                {
                    "name": "result.duration",
                    "value": "result.duration",
                    "aggregate": "sum"
                },
                {
                    "name": "result.failures",
                    "value": {"case": [
                        {"when": {"eq": {"result.ok": "F"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                },
                {
                    "name": "result.skips",
                    "value": {"case": [
                        {"when": {"eq": {"result.status": "SKIP"}}, "then": 1}
                    ]},
                    "aggregate": "sum",
                    "default": 0
                }
            ],
            "where": {"and": where_conditions}
        }
        t = ActiveDataThread(label, self, ad_query, path_mod)
        self.threads.append(t)

    def update_report(self, by_component, result, path_mod):
        def update_item(item, label, value):
            # It is important to include any existing item value in case ActiveData
            # returns multiple records for the same test; that can happen if the report
            # sometimes maps more than one ActiveData record to the same path.
            new_value = item.get(label, 0) + value
            if type(new_value) == int:
                item[label] = new_value
            else:
                item[label] = round(new_value, 2)

        if 'test' in result and 'tests' in by_component:
            test = result['test']
            if path_mod:
                test = path_mod(test)
            for bc in by_component['tests']:
                for item in by_component['tests'][bc]:
                    if test == item['test']:
                        seconds = round(result.get('duration', 0), 2)
                        update_item(item, 'total run time, seconds', seconds)
                        update_item(item, 'total runs', result.get('count', 0))
                        update_item(item, 'skipped runs', result.get('skips', 0))
                        update_item(item, 'failed runs', result.get('failures', 0))
                        return True
        return False

    def collect_activedata_results(self, by_component):
        # Start the first MAX_ACTIVEDATA_CONCURRENCY threads. If too many
        # concurrent requests are made to ActiveData, the requests frequently
        # fail (504 is the typical response).
        for i in range(min(MAX_ACTIVEDATA_CONCURRENCY, len(self.threads))):
            t = self.threads[i]
            t.start()
        # Wait for running threads (first N threads in self.threads) to complete.
        # When a thread completes, start the next thread, process the results
        # from the completed thread, and remove the completed thread from
        # the thread list.
        while len(self.threads):
            running_threads = min(MAX_ACTIVEDATA_CONCURRENCY, len(self.threads))
            for i in range(running_threads):
                t = self.threads[i]
                t.join(1)
                if not t.isAlive():
                    ad_response = t.response
                    path_mod = t.context
                    name = t.name
                    del self.threads[i]
                    if len(self.threads) >= MAX_ACTIVEDATA_CONCURRENCY:
                        running_threads = min(MAX_ACTIVEDATA_CONCURRENCY, len(self.threads))
                        self.threads[running_threads - 1].start()
                    if ad_response:
                        if len(ad_response) >= ACTIVEDATA_RECORD_LIMIT:
                            print("%s: ActiveData query limit reached; data may be missing" % name)
                        matches = 0
                        for record in ad_response:
                            if 'result' in record:
                                result = record['result']
                                if self.update_report(by_component, result, path_mod):
                                    matches += 1
                        self.log_verbose("%s: %d results; %d matches" %
                                         (name, len(ad_response), matches))
                        self.total_activedata_matches += matches
                    break

    def path_mod_reftest(self, path):
        # "<path1> == <path2>" -> "<path1>"
        path = path.split(' ')[0]
        # "<path>?<params>" -> "<path>"
        path = path.split('?')[0]
        # "<path>#<fragment>" -> "<path>"
        path = path.split('#')[0]
        return path

    def path_mod_jsreftest(self, path):
        # "<path>;assert" -> "<path>"
        path = path.split(';')[0]
        return path

    def path_mod_marionette(self, path):
        # "<path> <test-name>" -> "<path>"
        path = path.split(' ')[0]
        # "part1\part2" -> "part1/part2"
        path = path.replace('\\', os.path.sep)
        return path

    def path_mod_wpt(self, path):
        if path[0] == os.path.sep:
            # "/<path>" -> "<path>"
            path = path[1:]
        # "<path>" -> "testing/web-platform/tests/<path>"
        path = os.path.join('testing', 'web-platform', 'tests', path)
        # "<path>?<params>" -> "<path>"
        path = path.split('?')[0]
        return path

    def path_mod_jittest(self, path):
        # "part1\part2" -> "part1/part2"
        path = path.replace('\\', os.path.sep)
        # "<path>" -> "js/src/jit-test/tests/<path>"
        return os.path.join('js', 'src', 'jit-test', 'tests', path)

    def path_mod_xpcshell(self, path):
        # <manifest>.ini:<path> -> "<path>"
        path = path.split('.ini:')[-1]
        return path

    def add_activedata(self, branches, days, by_component):
        suites = {
            # List of known suites requiring special path handling and/or
            # suites typically containing thousands of test paths.
            # regexes have been selected by trial and error to partition data
            # into queries returning less than ACTIVEDATA_RECORD_LIMIT records.
            "reftest": (self.path_mod_reftest,
                        [{"regex": {"result.test": "layout/reftests/[a-k].*"}},
                         {"regex": {"result.test": "layout/reftests/[^a-k].*"}},
                         {"not": {"regex": {"result.test": "layout/reftests/.*"}}}]),
            "web-platform-tests": (self.path_mod_wpt,
                                   [{"regex": {"result.test": "/[a-g].*"}},
                                    {"regex": {"result.test": "/[h-p].*"}},
                                    {"not": {"regex": {"result.test": "/[a-p].*"}}}]),
            "web-platform-tests-reftest": (self.path_mod_wpt,
                                           [{"regex": {"result.test": "/css/css-.*"}},
                                            {"not": {"regex": {"result.test": "/css/css-.*"}}}]),
            "crashtest": (None,
                          [{"regex": {"result.test": "[a-g].*"}},
                           {"not": {"regex": {"result.test": "[a-g].*"}}}]),
            "web-platform-tests-wdspec": (self.path_mod_wpt, [None]),
            "web-platform-tests-crashtest": (self.path_mod_wpt, [None]),
            "xpcshell": (self.path_mod_xpcshell, [None]),
            "mochitest-plain": (None, [None]),
            "mochitest-browser-chrome": (None, [None]),
            "mochitest-media": (None, [None]),
            "mochitest-devtools-chrome": (None, [None]),
            "marionette": (self.path_mod_marionette, [None]),
            "mochitest-chrome": (None, [None]),
        }
        unsupported_suites = [
            # Usually these suites are excluded because currently the test resolver
            # does not provide test paths for them.
            "jsreftest",
            "jittest",
            "geckoview-junit",
            "cppunittest",
        ]
        for suite in suites:
            suite_clause = {"eq": {"run.suite.name": suite}}
            path_mod = suites[suite][0]
            test_clauses = suites[suite][1]
            suite_count = 1
            for test_clause in test_clauses:
                label = "%s-%d" % (suite, suite_count)
                suite_count += 1
                self.add_activedata_for_suite(label, branches, days,
                                              suite_clause, test_clause, path_mod)
        # Remainder: All supported suites not handled above.
        suite_clause = {"not": {"in": {"run.suite.name": unsupported_suites + list(suites)}}}
        self.add_activedata_for_suite("remainder", branches, days,
                                      suite_clause, None, None)
        self.collect_activedata_results(by_component)

    def description(self, components, flavor, subsuite, paths,
                    show_manifests, show_tests, show_summary, show_annotations,
                    show_activedata,
                    filter_values, filter_keys,
                    branches, days):
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
        if show_activedata:
            d += ", including historical run-time data for the last %d days on %s" % (
                days, branches)
        d += " as of %s." % datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
        return d

    def report(self, components, flavor, subsuite, paths,
               show_manifests, show_tests, show_summary, show_annotations,
               show_activedata,
               filter_values, filter_keys, show_components, output_file,
               branches, days):

        def matches_filters(test):
            '''
               Return True if all of the requested filter_values are found in this test;
               if filter_keys are specified, restrict search to those test keys.
            '''
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
        if not show_manifests and not show_tests and not show_summary and not show_annotations:
            show_manifests = True
            show_summary = True

        by_component = {}
        if components:
            components = components.split(',')
        if filter_keys:
            filter_keys = filter_keys.split(',')
        if filter_values:
            filter_values = filter_values.split(',')
        else:
            filter_values = []
        display_keys = (filter_keys or []) + ['skip-if', 'fail-if', 'fails-if']
        display_keys = set(display_keys)

        print("Finding tests...")
        here = os.path.abspath(os.path.dirname(__file__))
        resolver = TestResolver.from_environment(cwd=here, loader_cls=TestManifestLoader)
        tests = list(resolver.resolve_tests(paths=paths, flavor=flavor,
                                            subsuite=subsuite))

        manifest_paths = set()
        for t in tests:
            if 'manifest' in t and t['manifest'] is not None:
                manifest_paths.add(t['manifest'])
        manifest_count = len(manifest_paths)
        print("Resolver found {} tests, {} manifests".format(len(tests), manifest_count))

        if show_manifests:
            topsrcdir = self.build_obj.topsrcdir
            by_component['manifests'] = {}
            manifest_paths = list(manifest_paths)
            manifest_paths.sort()
            relpaths = []
            for manifest_path in manifest_paths:
                relpath = mozpath.relpath(manifest_path, topsrcdir)
                if mozpath.commonprefix((manifest_path, topsrcdir)) != topsrcdir:
                    continue
                relpaths.append(relpath)
            reader = self.build_obj.mozbuild_reader(config_mode='empty')
            files_info = reader.files_info(relpaths)
            for manifest_path in manifest_paths:
                relpath = mozpath.relpath(manifest_path, topsrcdir)
                if mozpath.commonprefix((manifest_path, topsrcdir)) != topsrcdir:
                    continue
                manifest_info = None
                if relpath in files_info:
                    bug_component = files_info[relpath].get('BUG_COMPONENT')
                    if bug_component:
                        key = "{}::{}".format(bug_component.product, bug_component.component)
                    else:
                        key = "<unknown bug component>"
                    if (not components) or (key in components):
                        manifest_info = {
                            'manifest': relpath,
                            'tests': 0,
                            'skipped': 0
                        }
                        rkey = key if show_components else 'all'
                        if rkey in by_component['manifests']:
                            by_component['manifests'][rkey].append(manifest_info)
                        else:
                            by_component['manifests'][rkey] = [manifest_info]
                if manifest_info:
                    for t in tests:
                        if t['manifest'] == manifest_path:
                            manifest_info['tests'] += 1
                            if t.get('skip-if'):
                                manifest_info['skipped'] += 1
            for key in by_component['manifests']:
                by_component['manifests'][key].sort()

        if show_tests:
            by_component['tests'] = {}

        if show_tests or show_summary or show_annotations:
            test_count = 0
            failed_count = 0
            skipped_count = 0
            annotation_count = 0
            condition_count = 0
            component_set = set()
            relpaths = []
            conditions = {}
            known_unconditional_annotations = ['skip', 'fail', 'asserts', 'random']
            known_conditional_annotations = ['skip-if', 'fail-if', 'run-if',
                                             'fails-if', 'fuzzy-if', 'random-if', 'asserts-if']
            for t in tests:
                relpath = t.get('srcdir_relpath')
                relpaths.append(relpath)
            reader = self.build_obj.mozbuild_reader(config_mode='empty')
            files_info = reader.files_info(relpaths)
            for t in tests:
                if not matches_filters(t):
                    continue
                if 'referenced-test' in t:
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
                            annotation_conditions = t[key].split(';')
                            for condition in annotation_conditions:
                                condition_count += 1
                                # Trim reftest fuzzy-if ranges: everything after the first comma
                                # eg. "Android,0-2,1-3" -> "Android"
                                condition = condition.split(',')[0]
                                if condition not in conditions:
                                    conditions[condition] = 0
                                conditions[condition] += 1
                test_count += 1
                relpath = t.get('srcdir_relpath')
                if relpath in files_info:
                    bug_component = files_info[relpath].get('BUG_COMPONENT')
                    if bug_component:
                        key = "{}::{}".format(bug_component.product, bug_component.component)
                    else:
                        key = "<unknown bug component>"
                    if (not components) or (key in components):
                        component_set.add(key)
                        test_info = {'test': relpath}
                        for test_key in display_keys:
                            value = t.get(test_key)
                            if value:
                                test_info[test_key] = value
                        if t.get('fail-if'):
                            failed_count += 1
                        if t.get('fails-if'):
                            failed_count += 1
                        if t.get('skip-if'):
                            skipped_count += 1
                        if show_tests:
                            rkey = key if show_components else 'all'
                            if rkey in by_component['tests']:
                                # Avoid duplicates: Some test paths have multiple TestResolver
                                # entries, as when a test is included by multiple manifests.
                                found = False
                                for ctest in by_component['tests'][rkey]:
                                    if ctest['test'] == test_info['test']:
                                        found = True
                                        break
                                if not found:
                                    by_component['tests'][rkey].append(test_info)
                            else:
                                by_component['tests'][rkey] = [test_info]
            if show_tests:
                for key in by_component['tests']:
                    by_component['tests'][key].sort(key=lambda k: k['test'])

        if show_activedata:
            try:
                self.add_activedata(branches, days, by_component)
            except Exception:
                print("Failed to retrieve some ActiveData data.")
                traceback.print_exc()
            self.log_verbose("%d tests updated with matching ActiveData data" %
                             self.total_activedata_matches)
            self.log_verbose("%d seconds waiting for ActiveData" %
                             self.total_activedata_seconds)

        by_component['description'] = self.description(
            components, flavor, subsuite, paths,
            show_manifests, show_tests, show_summary, show_annotations,
            show_activedata,
            filter_values, filter_keys,
            branches, days)

        if show_summary:
            by_component['summary'] = {}
            by_component['summary']['components'] = len(component_set)
            by_component['summary']['manifests'] = manifest_count
            by_component['summary']['tests'] = test_count
            by_component['summary']['failed tests'] = failed_count
            by_component['summary']['skipped tests'] = skipped_count

        if show_annotations:
            by_component['annotations'] = {}
            by_component['annotations']['total annotations'] = annotation_count
            by_component['annotations']['total conditions'] = condition_count
            by_component['annotations']['unique conditions'] = len(conditions)
            by_component['annotations']['conditions'] = conditions

        self.write_report(by_component, output_file)

        end_time = datetime.datetime.now()
        self.log_verbose("%d seconds total to generate report" %
                         (end_time - start_time).total_seconds())

    def write_report(self, by_component, output_file):
        json_report = json.dumps(by_component, indent=2, sort_keys=True)
        if output_file:
            output_file = os.path.abspath(output_file)
            output_dir = os.path.dirname(output_file)
            if not os.path.isdir(output_dir):
                os.makedirs(output_dir)

            with open(output_file, 'w') as f:
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

        by_component = {'tests': {}, 'summary': {}}
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
            by_component['summary']['%s delta' % key] = delta

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
        by_component['summary']['added tests'] = self.added_count
        by_component['summary']['deleted tests'] = self.deleted_count

    def diff_component(self, by_component, component, component1, component2):
        """
           Update by_component[component] with any added/deleted tests for the
           named component.
           "added": tests found in component2 but missing from component1.
           "deleted": tests found in component1 but missing from component2.
        """
        tests1 = set([t['test'] for t in component1])
        tests2 = set([t['test'] for t in component2])
        deleted = tests1 - tests2
        added = tests2 - tests1
        if deleted or added:
            by_component['tests'][component] = {}
            if deleted:
                by_component['tests'][component]['deleted'] = sorted(list(deleted))
            if added:
                by_component['tests'][component]['added'] = sorted(list(added))
        self.added_count += len(added)
        self.deleted_count += len(deleted)
        common = len(tests1.intersection(tests2))
        self.log_verbose("%s: %d deleted, %d added, %d common" % (component, len(deleted),
                                                                  len(added), common))
