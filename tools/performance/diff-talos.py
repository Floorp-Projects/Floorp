#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This is a simple script that does one thing only: compare talos runs from
two revisions. It is intended to check which of two try runs is best or if
a try improves over the m-c or m-i revision in branches from.

A big design goal is to avoid bit rot and to assert when bit rot is detected.
The set of tests we run is a moving target. When possible this script
should work with any test set, but in parts where it has to hard code
information, it should try to assert that it is valid so that changes
are detected and it is fixed earlier.
"""

import json
import urllib2
import math
import sys
from optparse import OptionParser

# FIXME: currently we assert that we know all the benchmarks just so
# we are sure to maintain the bigger_is_better set updated. Is there a better
# way to find/compute it?
bigger_is_better = frozenset(('v8_7', 'dromaeo_dom', 'dromaeo_css'))

smaller_is_better = frozenset(('tdhtmlr_paint', 'tp5n_main_rss_paint',
                               'ts_paint', 'tp5n_paint', 'tsvgr_opacity',
                               'a11yr_paint', 'kraken',
                               'tdhtmlr_nochrome_paint',
                               'tspaint_places_generated_med', 'tpaint',
                               'tp5n_shutdown_paint', 'tsvgr',
                               'tp5n_pbytes_paint', 'tscrollr_paint',
                               'tspaint_places_generated_max',
                               'tp5n_responsiveness_paint',
                               'sunspider', 'tp5n_xres_paint', 'num_ctors',
                               'tresize', 'trobopan', 'tcheckerboard',
                               'tcheck3', 'tcheck2', 'tprovider',
                               'tp5n_modlistbytes_paint',
                               'trace_malloc_maxheap', 'tp4m_nochrome',
                               'trace_malloc_leaks', 'tp4m_main_rss_nochrome',
                               'tp4m_shutdown_nochrome', 'tdhtml_nochrome',
                               'ts_shutdown', 'tp5n_%cpu_paint',
                               'trace_malloc_allocs', 'ts', 'codesighs',
                               'tsvg_nochrome', 'tp5n_content_rss_paint',
                               'tp5n_main_startup_fileio_paint',
                               'tp5n_nonmain_normal_netio_paint',
                               'tp5n_nonmain_startup_fileio_paint',
                               'tp5n_main_normal_fileio_paint',
                               'tp5n_nonmain_normal_fileio_paint',
                               'tp5n_main_startup_netio_paint',
                               'tp5n_main_normal_netio_paint',
                               'tp5n_main_shutdown_netio_paint',
                               'tp5n_main_shutdown_fileio_paint'))

all_benchmarks = smaller_is_better | bigger_is_better
assert len(smaller_is_better & bigger_is_better) == 0

def get_raw_data_for_revisions(revisions):
    """Loads data for the revisions, returns an array with one element for each
    revision."""
    selectors = ["revision=%s" % revision for revision in revisions]
    selector = '&'.join(selectors)
    url = "http://graphs.mozilla.org/api/test/runs/revisions?%s" % selector
    url_stream = urllib2.urlopen(url)
    data = json.load(url_stream)
    assert frozenset(data.keys()) == frozenset(('stat', 'revisions'))
    assert data['stat'] == 'ok'
    rev_data = data['revisions']
    assert frozenset(rev_data.keys()) == frozenset(revisions)
    return [rev_data[r] for r in revisions]

def mean(values):
    return float(sum(values))/len(values)

def c4(n):
    n = float(n)
    numerator = math.gamma(n/2)*math.sqrt(2/(n-1))
    denominator = math.gamma((n-1)/2)
    return numerator/denominator

def unbiased_standard_deviation(values):
    n = len(values)
    if n == 1:
        return None
    acc = 0
    avg = mean(values)
    for i in values:
        dist = i - avg
        acc += dist * dist
    return math.sqrt(acc/(n-1))/c4(n)

class BenchmarkResult:
    """ Stores the summary (mean and standard deviation) of a set of talus
    runs on the same revision and OS."""
    def __init__(self, avg, std):
        self.avg = avg
        self.std = std
    def __str__(self):
        t = "%s," % self.avg
        return "(%-13s %s)" % (t, self.std)

# FIXME: This function computes the statistics of multiple runs of talos on a
# single revision. Should it also support computing statistics over runs of
# different revisions assuming the revisions are equivalent from a performance
# perspective?
def digest_revision_data(data):
    ret = {}
    benchmarks = frozenset(data.keys())
    # assert that all the benchmarks are known. If they are not,
    # smaller_is_better or bigger_is_better needs to be updated depending on
    # the benchmark type.
    assert all_benchmarks.issuperset(benchmarks), \
        "%s not found in all_benchmarks" % ','.join((benchmarks - all_benchmarks))
    for benchmark in benchmarks:
        benchmark_data = data[benchmark]
        expected_keys = frozenset(("test_runs", "name", "id"))
        assert frozenset(benchmark_data.keys()) == expected_keys
        test_runs = benchmark_data["test_runs"]
        operating_systems = test_runs.keys()
        results = {}
        for os in operating_systems:
            os_runs = test_runs[os]
            values = []
            for os_run in os_runs:
                # there are 4 fields: test run id, build id, timestamp,
                # mean value
                assert len(os_run) == 4
                values.append(os_run[3])
            avg = mean(values)
            std = unbiased_standard_deviation(values)
            results[os] = BenchmarkResult(avg, std)
        ret[benchmark] = results
    return ret

def get_data_for_revisions(revisions):
    raw_data = get_raw_data_for_revisions(revisions)
    return [digest_revision_data(x) for x in raw_data]

def overlaps(a, b):
    return a[1] >= b[0] and b[1] >= a[0]

def is_significant(old, new):
    # conservative hack: if we don't know, say it is significant.
    if old.std is None or new.std is None:
        return True
    # use a 2 standard deviation interval, which is about 95% confidence.
    old_interval = [old.avg - old.std, old.avg + old.std]
    new_interval = [new.avg - new.std, new.avg + new.std]
    return not overlaps(old_interval, new_interval)

def compute_difference(benchmark, old, new):
    if benchmark in bigger_is_better:
        new, old = old, new

    if new.avg >= old.avg:
        return "%1.4fx worse" % (new.avg/old.avg)
    else:
        return "%1.4fx better" % (old.avg/new.avg)

#FIXME: the printing could use a table class that computes the sizes of the
# cells instead of the current hard coded values.
def print_data_comparison(datav):
    assert len(datav) == 2
    old_data = datav[0]
    new_data = datav[1]
    old_benchmarks = frozenset(old_data.keys())
    new_benchmarks = frozenset(new_data.keys())
    benchmarks = old_benchmarks.intersection(new_benchmarks)
    for benchmark in sorted(benchmarks):
        print benchmark
        old_benchmark_data = old_data[benchmark]
        new_benchmark_data = new_data[benchmark]
        old_operating_systems = frozenset(old_benchmark_data.keys())
        new_operating_systems = frozenset(new_benchmark_data.keys())
        operating_systems = old_operating_systems.intersection(new_operating_systems)
        for os in sorted(operating_systems):
            old_os_data = old_benchmark_data[os]
            new_os_data = new_benchmark_data[os]
            if not is_significant(old_os_data, new_os_data):
                continue

            diff = compute_difference(benchmark, old_os_data, new_os_data)
            print '%-33s | %-30s -> %-30s %s' % \
                (os, old_os_data, new_os_data, diff)
        print

def main():
    parser = OptionParser(usage='Usage: %prog old_revision new_revision')
    options, args = parser.parse_args()
    if len(args) != 2:
        parser.print_help()
        sys.exit(1)

    print_data_comparison(get_data_for_revisions(args))

if __name__ == '__main__':
    main()
