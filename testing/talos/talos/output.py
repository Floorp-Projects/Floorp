
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""output formats for Talos"""

import filter
import simplejson as json
import utils

from mozlog import get_proxy_logger

# NOTE: we have a circular dependency with output.py when we import results
import results as TalosResults

LOG = get_proxy_logger()


def filesizeformat(bytes):
    """
    Format the value like a 'human-readable' file size (i.e. 13 KB, 4.1 MB, 102
    bytes, etc).
    """
    bytes = float(bytes)
    formats = ('B', 'KB', 'MB')
    for f in formats:
        if bytes < 1024:
            return "%.1f%s" % (bytes, f)
        bytes /= 1024
    return "%.1fGB" % bytes  # has to be GB


class Output(object):
    """abstract base class for Talos output"""

    @classmethod
    def check(cls, urls):
        """check to ensure that the urls are valid"""

    def __init__(self, results):
        """
        - results : TalosResults instance
        """
        self.results = results

    def __call__(self):
        suites = []
        test_results = {
            'framework': {
                'name': self.results.results[0].framework,
            },
            'suites': suites,
        }

        for test in self.results.results:
            # serialize test results
            tsresult = None
            if not test.using_xperf:
                subtests = []
                suite = {
                    'name': test.name(),
                    'extraOptions': self.results.extra_options or [],
                    'subtests': subtests
                }

                suites.append(suite)
                vals = []
                replicates = {}

                # TODO: counters!!!! we don't have any, but they suffer the
                # same
                for result in test.results:
                    # XXX this will not work for manifests which list
                    # the same page name twice. It also ignores cycles
                    for page, val in result.raw_values():
                        if page == 'NULL':
                            page = test.name()
                            if tsresult is None:
                                tsresult = r = TalosResults.Results()
                                r.results = [{'index': 0, 'page': test.name(),
                                              'runs': val}]
                            else:
                                r = tsresult.results[0]
                                if r['page'] == test.name():
                                    r['runs'].extend(val)
                        replicates.setdefault(page, []).extend(val)

                tresults = [tsresult] if tsresult else test.results

                for result in tresults:
                    filtered_results = \
                        result.values(suite['name'],
                                      test.test_config['filters'])
                    vals.extend([[i['value'], j] for i, j in filtered_results])
                    for val, page in filtered_results:
                        if page == 'NULL':
                            # no real subtests
                            page = test.name()
                        subtest = {
                            'name': page,
                            'value': val['filtered'],
                            'replicates': replicates[page],
                        }
                        subtests.append(subtest)
                        if test.test_config.get('lower_is_better') is not None:
                            subtest['lowerIsBetter'] = \
                                test.test_config['lower_is_better']
                        if test.test_config.get('alert_threshold') is not None:
                            subtest['alertThreshold'] = \
                                test.test_config['alert_threshold']
                        if test.test_config.get('unit'):
                            subtest['unit'] = test.test_config['unit']

                # if there is more than one subtest, calculate a summary result
                if len(subtests) > 1:
                    suite['value'] = self.construct_results(
                        vals, testname=test.name())
                if test.test_config.get('lower_is_better') is not None:
                    suite['lowerIsBetter'] = \
                        test.test_config['lower_is_better']
                if test.test_config.get('alert_threshold') is not None:
                    suite['alertThreshold'] = \
                        test.test_config['alert_threshold']

            # counters results_aux data
            counter_subtests = []
            for cd in test.all_counter_results:
                for name, vals in cd.items():
                    # We want to add the xperf data as talos_counters
                    # exclude counters whose values are tuples (bad for
                    # graphserver)
                    if len(vals) > 0 and isinstance(vals[0], list):
                        continue

                    # mainthread IO is a list of filenames and accesses, we do
                    # not report this as a counter
                    if 'mainthreadio' in name:
                        continue

                    # responsiveness has it's own metric, not the mean
                    # TODO: consider doing this for all counters
                    if 'responsiveness' is name:
                        subtest = {
                            'name': name,
                            'value': filter.responsiveness_Metric(vals)
                        }
                        counter_subtests.append(subtest)
                        continue

                    subtest = {
                        'name': name,
                        'value': 0.0,
                    }
                    counter_subtests.append(subtest)

                    if test.using_xperf:
                        if len(vals) > 0:
                            subtest['value'] = vals[0]
                    else:
                        # calculate mean value
                        if len(vals) > 0:
                            varray = [float(v) for v in vals]
                            subtest['value'] = filter.mean(varray)
            if counter_subtests:
                suites.append({'name': test.name(),
                               'extraOptions': self.results.extra_options or [],
                               'subtests': counter_subtests})
        return test_results

    def output(self, results, results_url, tbpl_output):
        """output to the a file if results_url starts with file://
        - results : json instance
        - results_url : file:// URL
        """

        # parse the results url
        results_url_split = utils.urlsplit(results_url)
        results_scheme, results_server, results_path, _, _ = results_url_split

        if results_scheme in ('http', 'https'):
            self.post(results, results_server, results_path, results_scheme,
                      tbpl_output)
        elif results_scheme == 'file':
            with open(results_path, 'w') as f:
                for result in results:
                    f.write("%s\n" % result)
        else:
            raise NotImplementedError(
                "%s: %s - only http://, https://, and file:// supported"
                % (self.__class__.__name__, results_url)
            )

        # This is the output that treeherder expects to find when parsing the
        # log file
        if 'geckoProfile' not in self.results.extra_options:
            LOG.info("PERFHERDER_DATA: %s" % json.dumps(results,
                                                        ignore_nan=True))
        if results_scheme in ('file'):
            json.dump(results, open(results_path, 'w'), indent=2,
                      sort_keys=True, ignore_nan=True)

    def post(self, results, server, path, scheme, tbpl_output):
        raise NotImplementedError("Abstract base class")

    @classmethod
    def shortName(cls, name):
        """short name for counters"""
        names = {"Working Set": "memset",
                 "% Processor Time": "%cpu",
                 "Private Bytes": "pbytes",
                 "RSS": "rss",
                 "XRes": "xres",
                 "Modified Page List Bytes": "modlistbytes",
                 "Main_RSS": "main_rss"}
        return names.get(name, name)

    @classmethod
    def isMemoryMetric(cls, resultName):
        """returns if the result is a memory metric"""
        memory_metric = ['memset', 'rss', 'pbytes', 'xres', 'modlistbytes',
                         'main_rss', 'content_rss']  # measured in bytes
        return bool([i for i in memory_metric if i in resultName])

    @classmethod
    def v8_Metric(cls, val_list):
        results = [i for i, j in val_list]
        score = 100 * filter.geometric_mean(results)
        return score

    @classmethod
    def JS_Metric(cls, val_list):
        """v8 benchmark score"""
        results = [i for i, j in val_list]
        LOG.info("javascript benchmark")
        return sum(results)

    @classmethod
    def CanvasMark_Metric(cls, val_list):
        """CanvasMark benchmark score (NOTE: this is identical to JS_Metric)"""
        results = [i for i, j in val_list]
        LOG.info("CanvasMark benchmark")
        return sum(results)

    def construct_results(self, vals, testname):
        if 'responsiveness' in testname:
            return filter.responsiveness_Metric([val for (val, page) in vals])
        elif testname.startswith('v8_7'):
            return self.v8_Metric(vals)
        elif testname.startswith('kraken'):
            return self.JS_Metric(vals)
        elif testname.startswith('tcanvasmark'):
            return self.CanvasMark_Metric(vals)
        elif len(vals) > 1:
            return filter.geometric_mean([i for i, j in vals])
        else:
            return filter.mean([i for i, j in vals])
