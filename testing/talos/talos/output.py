
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""output formats for Talos"""

import filter
import json
import logging
import post_file
import time
import utils

# NOTE: we have a circular dependecy with output.py when we import results
import results as TalosResults

from StringIO import StringIO


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
        """return list of results strings"""
        raise NotImplementedError("Abstract base class")

    def output(self, results, results_url, tbpl_output):
        """output to the results_url
        - results_url : http:// or file:// URL
        - results : list of results
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
    def responsiveness_Metric(cls, val_list):
        return sum([float(x)*float(x) / 1000000.0 for x in val_list])

    @classmethod
    def v8_Metric(cls, val_list):
        results = [i for i, j in val_list]
        score = 100 * filter.geometric_mean(results)
        return score

    @classmethod
    def JS_Metric(cls, val_list):
        """v8 benchmark score"""
        results = [i for i, j in val_list]
        logging.info("javascript benchmark")
        return sum(results)

    @classmethod
    def CanvasMark_Metric(cls, val_list):
        """CanvasMark benchmark score (NOTE: this is identical to JS_Metric)"""
        results = [i for i, j in val_list]
        logging.info("CanvasMark benchmark")
        return sum(results)


class GraphserverOutput(Output):

    retries = 5  # number of times to attempt to contact graphserver
    info_format = ['title', 'testname', 'branch_name', 'sourcestamp',
                   'buildid', 'date']

    @classmethod
    def check(cls, urls):
        # ensure results_url link exists
        post_file.test_links(*urls)

    def __call__(self):
        """
        results to send to graphserver:
        construct all the strings of data, one string per test and one string
        per counter
        """

        result_strings = []

        info_dict = dict(
            title=self.results.title,
            date=self.results.date,
            branch_name=self.results.browser_config['branch_name'],
            sourcestamp=self.results.browser_config['sourcestamp'],
            buildid=self.results.browser_config['buildid'],
            browser_name=self.results.browser_config['browser_name'],
            browser_version=self.results.browser_config['browser_version']
        )

        for test in self.results.results:
            logging.debug("Working with test: %s", test.name())

            # get full name of test
            testname = test.name()
            if test.format == 'tpformat':
                # for some reason, we append the test extension to tp results
                # but not ts
                # http://hg.mozilla.org/build/talos/file/170c100911b6/talos
                # /run_tests.py#l176
                testname += test.extension()

            logging.debug("Generating results file: %s" % test.name())

            # HACK: when running xperf, we upload xperf counters to the graph
            # server but we do not want to
            # upload the test results as they will confuse the graph server
            if not (test.format == 'tpformat' and test.using_xperf):
                vals = []
                for result in test.results:
                    filtered_val = result.values(testname,
                                                 test.test_config['filters'])
                    vals.extend([[i['value'], j] for i, j in filtered_val])
                result_strings.append(self.construct_results(vals,
                                                             testname=testname,
                                                             **info_dict))

            # counter results
            for cd in test.all_counter_results:
                for counter_type, values in cd.items():
                    # get the counter name
                    counterName = '%s_%s' % (test.name(),
                                             self.shortName(counter_type))
                    if not values:
                        # failed to collect any data for this counter
                        logging.error(
                            "No results collected for: " + counterName
                        )
# NOTE: we are not going to enforce this warning for now as this happens too
# frequently: bugs 803413, 802475, 805925
#                        raise utils.TalosError("Unable to proceed with missing
# counter '%s'" % counterName)
# (jhammel: we probably should do this in e.g. results.py vs in
# graphserver-specific code anyway)

                    # exclude counters whose values are tuples (bad for
                    # graphserver)
                    if len(values) > 0 and isinstance(values[0], list):
                        print "Not uploading counter data for %s" % counterName
                        print values
                        continue

                    if test.mainthread() and 'mainthreadio' in counterName:
                        print ("Not uploading Mainthread IO data for %s"
                               % counterName)
                        print values
                        continue

                    # counter values
                    vals = [[x, 'NULL'] for x in values]

                    # append test name extension but only for tpformat tests
                    if test.format == 'tpformat':
                        counterName += test.extension()

                    info = info_dict.copy()
                    info['testname'] = counterName

                    # append the counter string
                    logging.info(
                        "Generating results file: %s" % counterName)
                    result_strings.append(self.construct_results(vals, **info))

        return result_strings

    def responsiveness_test(self, testname):
        """returns if the test is a responsiveness test"""
        # XXX currently this just looks for the string
        # 'responsiveness' in the test name.
        # It would be nice to be more declarative about this
        return 'responsiveness' in testname

    def construct_results(self, vals, testname, **info):
        """
        return results string appropriate to graphserver
        - vals: list of 2-tuples: [(val, page)
        - kwargs: info necessary for self.info_format interpolation
        see https://wiki.mozilla.org/Buildbot/Talos/DataFormat
        """

        info['testname'] = testname
        info_format = self.info_format
        responsiveness = self.responsiveness_test(testname)
        _type = 'VALUES'
        average = None
        if responsiveness:
            _type = 'AVERAGE'
            average = self.responsiveness_Metric([val for (val, page) in vals])
        elif testname.startswith('v8_7'):
            _type = 'AVERAGE'
            average = self.v8_Metric(vals)
        elif testname.startswith('kraken'):
            _type = 'AVERAGE'
            average = self.JS_Metric(vals)
        elif testname.startswith('tcanvasmark'):
            _type = 'AVERAGE'
            average = self.CanvasMark_Metric(vals)

        # ensure that we have all of the info data available
        missing = [i for i in info_format if i not in info]
        if missing:
            raise utils.TalosError("Missing keys: %s" % missing)
        info = ','.join([str(info[key]) for key in info_format])

        # write the data
        buffer = StringIO()
        buffer.write("START\n")
        buffer.write("%s\n" % _type)
        buffer.write('%s\n' % info)
        if average is not None:
            # write some kind of average
            buffer.write("%s\n" % average)
        else:
            for i, (val, page) in enumerate(vals):
                try:
                    buffer.write("%d,%.2f,%s\n" % (i, float(val), page))
                except ValueError:
                    logging.info(
                        "We expected a numeric value and recieved '%s' instead"
                        % val
                    )
                    pass

        buffer.write("END")
        return buffer.getvalue()

    def process_Request(self, post):
        """get links from the graphserver response"""
        links = ""
        for line in post.splitlines():
            if line.find("RETURN\t") > -1:
                line = line.replace("RETURN\t", "")
                links += line + '\n'
            logging.debug("process_Request line: %s", line)
        if not links:
            raise utils.TalosError("send failed, graph server says:\n%s"
                                   % post)
        return links

    def post(self, results, server, path, scheme, tbpl_output):
        """post results to the graphserver"""

        links = []
        wait_time = 5  # number of seconds between each attempt

        for index, data_string in enumerate(results):
            times = 0
            msg = ""
            while times < self.retries:
                logging.info(
                    "Posting result %d of %d to %s://%s%s, attempt %d",
                    index, len(results), scheme, server, path, times)
                try:
                    links.append(self.process_Request(
                        post_file.post_multipart(server, path,
                                                 files=[("filename",
                                                         "data_string",
                                                         data_string)])))
                    break
                except utils.TalosError, e:
                    msg = str(e)
                except Exception, e:
                    msg = str(e)
                times += 1
                time.sleep(wait_time)
                wait_time *= 2
            else:
                raise utils.TalosError(
                    "Graph server unreachable (%d attempts)\n%s"
                    % (self.retries, msg)
                )

        # add TBPL output
        self.add_tbpl_output(links, tbpl_output, server, scheme)

    def add_tbpl_output(self, links, tbpl_output, server, scheme):
        """
        add graphserver links such that TBPL can parse them.
        graphserver returns a response like:

          'tsvgr\tgraph.html#tests=[[224,113,14]]\ntsvgr\t2965.75\tgraph.html
          #tests=[[224,113,14]]\n'

        for each ts posted (tsvgr, in this case)
        """

        url_format = "%s://%s/%s"

        # XXX this will not work for multiple URLs :(
        tbpl_output.setdefault('graphserver', {})

        # XXX link_format to be deprecated; see
        # https://bugzilla.mozilla.org/show_bug.cgi?id=816634
        link_format = '<a href=\'%s\'>%s</a>'

        for response in links:

            # parse the response:
            # graphserver returns one of two responses.  For 'AVERAGE' payloads
            # graphserver returns a line
            # 'RETURN\t<test name>\t<value>\t<path segment>' :
            # http://hg.mozilla.org/graphs/file/8884ef9418bf/server/pyfomatic
            # /collect.py#l277
            # For 'VALUES' payloads, graphserver prepends an additional line
            # 'RETURN\t<test name>\t<path segment>' :
            # http://hg.mozilla.org/graphs/file/8884ef9418bf/server/pyfomatic
            # /collect.py#l274
            # see https://bugzilla.mozilla.org/show_bug.cgi?id=816634#c56 for
            # a more verbose explanation
            lines = [line.strip() for line in response.strip().splitlines()]
            assert len(lines) in (1, 2), """\
Should have one line for 'AVERAGE' payloads,
two lines for 'VALUES' payloads. You received:
%s""" % lines
            testname, result, path = lines[-1].split()
            if self.isMemoryMetric(testname):
                result = filesizeformat(result)

            # add it to the output
            url = url_format % (scheme, server, path)
            tbpl_output['graphserver'][testname] = {'url': url,
                                                    'result': result}

            # output to legacy TBPL; to be deprecated, see
            # https://bugzilla.mozilla.org/show_bug.cgi?id=816634
            linkName = '%s: %s' % (testname, result)
            print 'RETURN: %s' % link_format % (url, linkName)


class PerfherderOutput(Output):
    def __init__(self, results):
        Output.__init__(self, results)

    def output(self, results, results_url, tbpl_output):
        """output to the a file if results_url starts with file://
        - results : json instance
        - results_url : file:// URL
        """

        # parse the results url
        results_url_split = utils.urlsplit(results_url)
        results_scheme, results_server, results_path, _, _ = results_url_split

        # This is the output that treeherder expects to find when parsing the
        # log file
        logging.info("PERFHERDER_DATA: %s" % json.dumps(results))
        if results_scheme in ('file'):
            json.dump(results, file(results_path, 'w'), indent=2,
                      sort_keys=True)

    def post(self, results, server, path, scheme, tbpl_output):
        """conform to current code- not needed for perfherder"""
        pass

    def construct_results(self, vals, testname):
        if 'responsiveness' in testname:
            return self.responsiveness_Metric([val for (val, page) in vals])
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

    def __call__(self):
        suites = []
        test_results = {
            'framework': {
                'name': 'talos',
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
                    'subtests': subtests,
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
                        if test.test_config.get('unit'):
                            subtest['unit'] = test.test_config['unit']

                # if there is more than one subtest, calculate a summary result
                if len(subtests) > 1:
                    suite['value'] = self.construct_results(
                        vals, testname=test.name())
                if test.test_config.get('lower_is_better') is not None:
                    suite['lowerIsBetter'] = \
                        test.test_config['lower_is_better']

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

                    subtest = {
                        'name': name,
                        'value': 0.0,
                    }
                    counter_subtests.append(subtest)

                    if test.using_xperf:
                        subtest['value'] = vals[0]
                    else:
                        # calculate mean value
                        if len(vals) > 0:
                            varray = [float(v) for v in vals]
                            subtest['value'] = filter.mean(varray)
            if counter_subtests:
                suites.append({'name': test.name(),
                               'subtests': counter_subtests})
        return test_results

# available output formats
formats = {'datazilla_urls': PerfherderOutput,
           'results_urls': GraphserverOutput}
