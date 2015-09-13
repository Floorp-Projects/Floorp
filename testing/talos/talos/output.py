# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""output formats for Talos"""

import filter
import json
import mozinfo
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
                    vals.extend([[i['filtered'], j] for i, j in filtered_val])
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
        logging.info("TALOSDATA: %s" % json.dumps(results))
        if results_scheme in ('file'):
            json.dump(results, file(results_path, 'w'), indent=2,
                      sort_keys=True)

    def post(self, results, server, path, scheme, tbpl_output):
        """conform to current code- not needed for perfherder"""
        pass

    # TODO: this is copied directly from the old datazilla output. Using it
    # as we have established platform names already
    def test_machine(self):
        """return test machine platform in a form appropriate to datazilla"""
        platform = mozinfo.os
        version = mozinfo.version
        processor = mozinfo.processor
        if self.results.title.endswith(".e") and \
                not version.endswith('.e'):
            # we are running this against e10s builds
            version = '%s.e' % (version,)

        return dict(name=self.results.title, os=platform, osversion=version,
                    platform=processor)

    # TODO: this is copied from datazilla output code, do we need all of this?
    def run_options(self, test):
        """test options for datazilla"""

        options = {}
        test_options = ['rss', 'cycles', 'tpmozafterpaint', 'responsiveness',
                        'shutdown']
        if 'tpmanifest' in test.test_config:
            test_options += ['tpchrome', 'tpcycles', 'tppagecycles',
                             'tprender', 'tploadaboutblank', 'tpdelay']

        for option in test_options:
            if option not in test.test_config:
                continue
            options[option] = test.test_config[option]
        if test.extensions is not None:
            options['extensions'] = [{'name': extension}
                                     for extension in test.extensions]
        return options

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
        # platform
        machine = self.test_machine()

        # build information
        browser_config = self.results.browser_config

        test_results = []

        for test in self.results.results:
            test_result = {
                'test_machine': {},
                'testrun': {},
                'results': {},
                'talos_counters': {},
                'test_build': {}
            }

            test_result['testrun']['suite'] = test.name()
            test_result['testrun']['options'] = self.run_options(test)
            test_result['testrun']['date'] = self.results.date

            # serialize test results
            results = {}
            tsresult = None
            summary = {"suite": 0, "subtests": {}}
            if not test.using_xperf:
                vals = []

                # TODO: counters!!!! we don't have any, but they suffer the same
                for result in test.results:
                    # XXX this will not work for manifests which list
                    # the same page name twice. It also ignores cycles
                    for page, val in result.raw_values():
                        if page == 'NULL':
                            results.setdefault(test.name(), []).extend(val)
                            if tsresult is None:
                                tsresult = r = TalosResults.Results()
                                r.results = [{'index': 0, 'page': test.name(),
                                              'runs': val}]
                            else:
                                r = tsresult.results[0]
                                if r['page'] == test.name():
                                    r['runs'].extend(val)
                        else:
                            results.setdefault(page, []).extend(val)

                tresults = [tsresult] if tsresult else test.results

                for result in tresults:
                    filtered_results = \
                        result.values(test_result['testrun']['suite'],
                                      test.test_config['filters'])
                    vals.extend([[i['filtered'], j] for i, j in filtered_results])
                    for val, page in filtered_results:
                        if page == 'NULL':
                            summary['subtests'][test.name()] = val
                        else:
                            summary['subtests'][page] = val


                suite_summary = self.construct_results(vals,
                                                       testname=test.name())
                summary['suite'] = suite_summary
                test_result['summary'] = summary

                for result, values in results.items():
                    test_result['results'][result] = values

            # counters results_aux data
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

                    if test.using_xperf:
                        test_result['talos_counters'][name] = {"mean": vals[0]}
                    else:
                        # calculate mean and max value
                        varray = []
                        counter_mean = 0
                        counter_max = 0
                        if len(vals) > 0:
                            for v in vals:
                                varray.append(float(v))
                            counter_mean = "%.2f" % filter.mean(varray)
                            counter_max = "%.2f" % max(varray)
                        test_result['talos_counters'][name] = {
                            "mean": counter_mean,
                            "max": counter_max
                        }

            if browser_config['develop'] and not browser_config['sourcestamp']:
                browser_config['sourcestamp'] = ''

            test_result['test_build'] = {
                'version': browser_config['browser_version'],
                'revision': browser_config['sourcestamp'],
                'id': browser_config['buildid'],
                'branch': browser_config['branch_name'],
                'name': browser_config['browser_name']
            }

            test_result['test_machine'] = {
                'platform': machine['platform'],
                'osversion': machine['osversion'],
                'os': machine['os'],
                'name': machine['name']
            }

            test_results.append(test_result)
        return test_results

# available output formats
formats = {'datazilla_urls': PerfherderOutput,
           'results_urls': GraphserverOutput}
