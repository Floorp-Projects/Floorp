# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import urllib
import httplib
import datetime
import time
from argparse import ArgumentParser
import sys
import os
import filter

SERVER = 'graphs.mozilla.org'
selector = '/api/test/runs'
debug = 1

branch_map = {}
branch_map['Try'] = {
    'pgo': {
        'id': 23, 'name': 'Try'
    },
    'nonpgo': {
        'id': 113, 'name': 'Try'
    }
}
branch_map['Firefox'] = {
    'pgo': {
        'id': 1, 'name': 'Firefox'
    },
    'nonpgo': {
        'id': 94, 'name': 'Firefox-Non-PGO'
    }
}
branch_map['Inbound'] = {
    'pgo': {
        'id': 63, 'name': 'Mozilla-Inbound'
    },
    'nonpgo': {
        'id': 131, 'name': 'Mozilla-Inbound-Non-PGO'
    }
}
branch_map['Aurora'] = {
    'pgo': {
        'id': 52, 'name': 'Mozilla-Aurora'
    }
}
branch_map['Beta'] = {
    'pgo': {
        'id': 53, 'name': 'Mozilla-Beta'
    }
}
branch_map['Cedar'] = {
    'pgo': {
        'id': 26, 'name': 'Cedar'
    },
    'nonpgo': {
        'id': 26, 'name': 'Cedar'
    }
}
branch_map['UX'] = {
    'pgo': {
        'id': 59, 'name': 'UX'
    },
    'nonpgo': {
        'id': 137, 'name': 'UX-Non-PGO'
    }
}
branches = ['Try', 'Firefox', 'Inbound', 'Aurora', 'Beta', 'Cedar', 'UX']

# TODO: pull test names and reverse_tests from test.py in the future
test_map = {}
test_map['dromaeo_css'] = {'id': 72, 'tbplname': 'dromaeo_css'}
test_map['dromaeo_dom'] = {'id': 73, 'tbplname': 'dromaeo_dom'}
test_map['kraken'] = {'id': 232, 'tbplname': 'kraken'}
test_map['v8_7'] = {'id': 230, 'tbplname': 'v8_7'}
test_map['a11yr'] = {'id': 223, 'tbplname': 'a11yr_paint'}
test_map['ts_paint'] = {'id': 83, 'tbplname': 'ts_paint'}
test_map['tpaint'] = {'id': 82, 'tbplname': 'tpaint'}
test_map['tsvgr_opacity'] = {'id': 225, 'tbplname': 'tsvgr_opacity'}
test_map['tresize'] = {'id': 254, 'tbplname': 'tresize'}
test_map['tp5n'] = {'id': 206, 'tbplname': 'tp5n_paint'}
test_map['tp5o'] = {'id': 255, 'tbplname': 'tp5o_paint'}
test_map['tp5o_scroll'] = {'id': 323, 'tbplname': 'tp5o_scroll'}
test_map['tsvgx'] = {'id': 281, 'tbplname': 'tsvgx'}
test_map['tscrollx'] = {'id': 287, 'tbplname': 'tscrollx'}
test_map['sessionrestore'] = {'id': 313, 'tbplname': 'sessionrestore'}
test_map['sessionrestore_no_auto_restore'] = \
    {'id': 315, 'tbplname': 'sessionrestore_no_auto_restore'}
test_map['tart'] = {'id': 293, 'tbplname': 'tart'}
test_map['cart'] = {'id': 309, 'tbplname': 'cart'}
test_map['damp'] = {'id': 327, 'tbplname': 'damp'}
test_map['tcanvasmark'] = {'id': 289, 'tbplname': 'tcanvasmark'}
test_map['glterrain'] = {'id': 325, 'tbplname': 'glterrain'}
test_map['media_tests'] = {'id': 317, 'tbplname': 'media_tests'}

tests = ['tresize', 'kraken', 'v8_7', 'dromaeo_css', 'dromaeo_dom', 'a11yr',
         'ts_paint', 'tpaint', 'tsvgr_opacity', 'tp5n', 'tp5o', 'tart',
         'tcanvasmark', 'tsvgx', 'tscrollx', 'sessionrestore',
         'sessionrestore_no_auto_restore', 'glterrain', 'cart', 'tp5o_scroll',
         'media_tests', 'damp']
reverse_tests = ['dromaeo_css', 'dromaeo_dom', 'v8_7', 'canvasmark']

platform_map = {}
# 14 - 14 is the old fedora, we are now on Ubuntu slaves
platform_map['Linux'] = 33
platform_map['Linux (e10s)'] = 41
# 15 - 15 is the old fedora, we are now on Ubuntu slaves
platform_map['Linux64'] = 35
platform_map['Linux64 (e10s)'] = 43
platform_map['Win7'] = 25  # 12 is for non-ix
platform_map['Win7 (e10s)'] = 47
platform_map['Win8'] = 31
platform_map['Win8 (e10s)'] = 49
platform_map['WinXP'] = 37  # 1 is for non-ix
platform_map['WinXP (e10s)'] = 45
platform_map['OSX10.10'] = 55
platform_map['OSX10.10 (e10s)'] = 57
platforms = ['Linux', 'Linux64', 'Win7', 'WinXP', 'Win8', 'OSX10.10']
platforms_e10s = ['Linux (e10s)', 'Linux64 (e10s)', 'Win7 (e10s)',
                  'WinXP (e10s)', 'Win8 (e10s)', 'OSX10.10 (e10s)']

disabled_tests = {}
disabled_tests['Linux'] = ['media_tests', 'tp5n']
disabled_tests['Linux (e10s)'] = ['media_tests', 'tp5n']
disabled_tests['Win7'] = ['media_tests']
disabled_tests['Win7 (e10s)'] = ['media_tests']
disabled_tests['Win8'] = ['media_tests', 'tp5n']
disabled_tests['Win8 (e10s)'] = ['media_tests', 'tp5n']
disabled_tests['WinXP'] = ['media_tests', 'dromaeo_dom', 'dromaeo_css',
                           'kraken', 'v8_7', 'tp5n']
disabled_tests['WinXP (e10s)'] = ['media_tests', 'dromaeo_dom', 'dromaeo_css',
                                  'kraken', 'v8_7', 'tp5n']
disabled_tests['Win64'] = ['media_tests', 'tp5n']
disabled_tests['OSX10.10'] = ['media_tests', 'tp5n']
disabled_tests['OSX10.10 (e10s)'] = ['media_tests', 'tp5n']


def getListOfTests(platform, tests):
    if platform in disabled_tests:
        skip_tests = disabled_tests[platform]
        for t in skip_tests:
            if t in tests:
                tests.remove(t)
    return tests


def getGraphData(testid, branchid, platformid):
    body = {"id": testid, "branchid": branchid, "platformid": platformid}
    if debug >= 3:
        print "Querying graph server for: %s" % body
    params = urllib.urlencode(body)
    headers = {
        "Content-type": "application/x-www-form-urlencoded",
        "Accept": "text/plain"
    }
    conn = httplib.HTTPConnection(SERVER)
    conn.request("POST", selector, params, headers)
    response = conn.getresponse()
    data = response.read()

    if data:
        try:
            data = json.loads(data)
        except:
            print "NOT JSON: %s" % data
            return None

    if data['stat'] == 'fail':
        return None
    return data


# TODO: consider moving this to mozinfo or datazilla_client
def getDatazillaPlatform(os, platform, osversion, product):
    platform = None
    if product == 'Fennec':
        platform = 'Tegra'

    if os == 'linux':
        if platform == 'x86_64':
            platform = 'Linux64'
        else:
            platform = 'Linux'
    elif os == 'win':
        if osversion == '6.1.7600' or osversion == '6.1.7601':
            platform = 'Win'
        elif osversion == '6.2.9200':
            platform = 'Win8'
        elif osversion == '6.2.9200.m':
            platform = 'Win8.m'
        else:
            platform = 'WinXP'
    elif os == 'mac':
        if osversion.startswith('OS X 10.10'):
            platform = 'OSX10.10'
    return platform


# TODO: move this to datazilla_client.
def getDatazillaCSET(revision, branchid):
    testdata = {}
    pgodata = {}
    xperfdata = {}
    cached = "%s.json" % revision
    if os.path.exists(cached):
        response = open(cached, 'r')
    else:
        conn = httplib.HTTPSConnection('datazilla.mozilla.org')
        cset = "/talos/testdata/raw/%s/%s" % (branchid, revision)
        conn.request("GET", cset)
        response = conn.getresponse()

    data = response.read()
    response.close()
    cdata = json.loads(data)

    for testrun in cdata:
        values = []
        platform = getDatazillaPlatform(testrun['test_machine']['os'],
                                        testrun['test_machine']['platform'],
                                        testrun['test_machine']['osversion'],
                                        testrun['test_build']['name'])

        if platform not in testdata:
            testdata[platform] = {}

        if platform not in pgodata:
            pgodata[platform] = {}

        pgo = True
        if 'Non-PGO' in testrun['test_build']['branch']:
            pgo = False

        suite = testrun['testrun']['suite']
        extension = ''
        if not testrun['testrun']['options']['tpchrome']:
            extension = "%s_nochrome" % extension

        # these test names are reported without _paint
        if 'svg' not in suite and \
           'kraken' not in suite and \
           'sunspider' not in suite and \
           'dromaeo' not in suite and \
           testrun['testrun']['options']['tpmozafterpaint']:
            if 'paint' not in suite:  # handle tspaint_places_generated_*
                extension = "%s_paint" % extension

        suite = "%s%s" % (suite, extension)

        # This is a hack that means we are running xperf since tp5n was
        # replaced by tp5o
        if platform == "Win" and suite == "tp5n_paint":
            xperfdata = testrun['results_xperf']

        for page in testrun['results']:

            index = 0
            if len(testrun['results'][page]) > 0:
                index = 0

            vals = sorted(testrun['results'][page][index:])[:-1]

            if len(vals) == 0:
                vals = [0]
            values.append(float(sum(vals))/len(vals))

        # ignore max
        if len(values) >= 2:
            values = sorted(values)[:-1]
        if len(values) == 0:
            values = [0]

        # mean
        if pgo:
            pgodata[platform][suite] = float(sum(values)/len(values))
            if debug > 1:
                print "%s: %s: %s (PGO)" % (platform, suite,
                                            pgodata[platform][suite])
        else:
            testdata[platform][suite] = float(sum(values)/len(values))
            if debug > 1:
                print "%s: %s: %s" % (platform, suite,
                                      testdata[platform][suite])

    return testdata, pgodata, xperfdata


def getDatazillaData(branchid):
    # TODO: resolve date, currently days_ago=7,

    # https://datazilla.mozilla.org/refdata/pushlog/list
    # /?days_ago=7&branches=Mozilla-Inbound
    conn = httplib.HTTPSConnection('datazilla.mozilla.org')
    cset = "/refdata/pushlog/list/?days_ago=14&branches=%s" % branchid
    conn.request("GET", cset)
    response = conn.getresponse()
    data = response.read()
    jdata = json.loads(data)
    alldata = {}

    for item in jdata:
        alldata[jdata[item]['revisions'][0]] = \
            getDatazillaCSET(jdata[item]['revisions'][0], branchid)
    return alldata


def parseGraphResultsByDate(data, start, end):
    low = sys.maxint
    high = 0
    count = 0
    runs = data['test_runs']
    vals = []
    dataid = 4  # 3 for average, 4 for geomean
    for run in runs:
        if run[2] >= start and run[2] <= end:
            vals.append(run[dataid])
            if run[dataid] < low:
                low = run[dataid]
            if run[dataid] > high:
                high = run[dataid]
            count += 1

    average = 0
    geomean = 0
    if count > 0:
        average = filter.mean(vals)
        geomean = filter.geometric_mean(vals)
    return {'low': low, 'high': high, 'avg': average, 'geomean': geomean,
            'count': count, 'data': vals}


def parseGraphResultsByChangeset(data, changeset):
    low = sys.maxint
    high = 0
    count = 0
    runs = data['test_runs']
    vals = []
    dataid = 7  # 3 for average, 7 for geomean
    for run in runs:
        push = run[1]
        cset = push[2]
        if cset == changeset:
            vals.append(run[dataid])
            if run[dataid] < low:
                low = run[dataid]
            if run[dataid] > high:
                high = run[dataid]
            count += 1

    average = 0
    geomean = 0
    if count > 0:
        average = filter.mean(vals)
        geomean = filter.geometric_mean(vals)
    return {'low': low, 'high': high, 'avg': average, 'geomean': geomean,
            'count': count, 'data': vals}


def compareResults(revision, branch, masterbranch, skipdays, history,
                   platforms, tests, pgo=False, printurl=False,
                   compare_e10s=False, dzdata=None, pgodzdata=None,
                   verbose=False, masterrevision=None, doPrint=False):
    startdate = int(
        time.mktime((datetime.datetime.now() -
                     datetime.timedelta(days=(skipdays+history))).timetuple())
    )
    enddate = int(
        time.mktime((datetime.datetime.now() -
                     datetime.timedelta(days=skipdays)).timetuple())
    )

    if doPrint:
        print ("   test      Master     gmean  stddev points  change     "
               " gmean  stddev points  graph-url")

    for p in platforms:
        output = ["\n%s:" % p]
        itertests = getListOfTests(p, tests)

        for t in itertests:
            dzval = None
            if dzdata:
                if p in dzdata:
                    if test_map[t]['tbplname'] in dzdata[p]:
                        dzval = dzdata[p][test_map[t]['tbplname']]

            pgodzval = None
            if pgodzdata:
                if p in pgodzdata:
                    if test_map[t]['tbplname'] in pgodzdata[p]:
                        pgodzval = pgodzdata[p][test_map[t]['tbplname']]

            if p.startswith('OSX') or pgo:
                test_bid = branch_map[branch]['pgo']['id']
            else:
                test_bid = branch_map[branch]['nonpgo']['id']

            if p.startswith('OSX') or pgo:
                bid = branch_map[masterbranch]['pgo']['id']
            else:
                bid = branch_map[masterbranch]['nonpgo']['id']

            e10s = True
            plat = p
            if e10s:
                plat = "%s (e10s)" % p
            data = getGraphData(test_map[t]['id'], bid, platform_map[plat])

            if not e10s and compare_e10s:
                plat = "%s (e10s)" % p
            testdata = getGraphData(test_map[t]['id'],
                                    test_bid, platform_map[plat])

            if data and testdata:
                if masterrevision:
                    results = \
                        parseGraphResultsByChangeset(data, masterrevision)
                else:
                    results = parseGraphResultsByDate(data, startdate, enddate)
                test = parseGraphResultsByChangeset(testdata, revision)
                status = ''
                if test['geomean'] < results['low']:
                    status = ':)'
                    if t in reverse_tests:
                        status = ':('
                if test['geomean'] > results['high']:
                    status = ':('
                    if t in reverse_tests:
                        status = ':)'

                if test['low'] == sys.maxint or \
                        results['low'] == sys.maxint or \
                        test['high'] == 0 or \
                        results['high'] == 0:
                    output.append("   %-18s    No results found" % t)
                else:
                    string = "%2s %-18s" % (status, t)
                    for d in [results, test]:
                        string += (" %7.1f +/- %2.0f%% (%s#)"
                                   % (d['geomean'],
                                      (100 * filter.stddev(d['data']) /
                                       d['geomean']), (len(d['data']))))
                        if d == results:
                            string += "  [%+6.1f%%]  " % (
                                ((test['geomean'] / results['geomean']) - 1) *
                                100
                            )

                    if dzval and pgodzval:
                        string = "    [%s] [PGO: %s]" % (dzval, pgodzval)
                    if printurl:
                        urlbase = 'http://graphs.mozilla.org/graph.html#tests='
                        points = []
                        if compare_e10s:
                            points.append("[%s,%s,%s]" % (
                                test_map[t]['id'],
                                bid,
                                platform_map[p + " (e10s)"]
                            ))
                        points.append("[%s,%s,%s]" % (test_map[t]['id'],
                                                      bid,
                                                      platform_map[p]))
                        string += "    %s" % shorten("%s[%s]" % (
                            urlbase,
                            ','.join(points)
                        ))
                    # Output data for all tests in case of --verbose.
                    # If not --verbose, then output in case of improvements
                    # or regression.
                    if verbose or status != '':
                        output.append(string)
            else:
                output.append("   %-18s    No data for platform" % t)

        if doPrint:
            print '\n'.join(output)


class CompareOptions(ArgumentParser):

    def __init__(self):
        ArgumentParser.__init__(self)

        self.add_argument("--revision",
                          action="store", type=str, dest="revision",
                          default=None,
                          help="revision of the source you are testing")

        self.add_argument("--master-revision",
                          action="store", type=str, dest="masterrevision",
                          default=None,
                          help="revision of the masterbranch if you want to"
                               " compare to a single push")

        self.add_argument("--branch",
                          action="store", type=str, dest="branch",
                          default="Try",
                          help="branch that your revision landed on which you"
                               " are testing, default 'Try'."
                               " Options are: %s" % (branches))

        self.add_argument("--masterbranch",
                          action="store", type=str, dest="masterbranch",
                          default="Firefox",
                          help="master branch that you will be comparing"
                               " against, default 'Firefox'."
                               " Options are: %s" % (branches))

        self.add_argument("--skipdays",
                          action="store", type=int, dest="skipdays",
                          default=0,
                          help="Specify the number of days to ignore results,"
                               " default '0'.  Note: If a regression landed 4"
                               " days ago, use --skipdays=5")

        self.add_argument("--history",
                          action="store", type=int, dest="history",
                          default=14,
                          help="Specify the number of days in history to go"
                               " back and get results.  If specified in"
                               " conjunction with --skipdays, we will collect"
                               " <history> days starting an extra <skipdays>"
                               " in the past.  For example, if you have"
                               " skipdays as 7 and history as 14, then we will"
                               " collect data from 7-21 days in the past."
                               " Default is 14 days")

        self.add_argument("--platform",
                          action="append", type=str, dest="platforms",
                          metavar="PLATFORMS", default=None, choices=platforms,
                          help="Specify a single platform to compare. This"
                               " option can be specified multiple times and"
                               " defaults to 'All' if not specified."
                               " Options are: %s" % (platforms))

        self.add_argument("--testname",
                          action="append", type=str, dest="testnames",
                          metavar="TESTS", default=None, choices=tests,
                          help="Specify a single test to compare. This option"
                               " can be specified multiple times and defaults"
                               " to 'All' if not specified. Options are: %s"
                               % (tests))

        self.add_argument("--print-graph-url",
                          action="store_true", dest="printurl",
                          default=False,
                          help="Print a url that can link to the data in graph"
                               " server")

        self.add_argument("--pgo",
                          action="store_true", dest="pgo",
                          default=False,
                          help="Use PGO Branch if available")

        self.add_argument("--compare-e10s",
                          action="store_true", dest="compare_e10s",
                          default=False,
                          help="Compare e10s vs non-e10s")

        self.add_argument("--xperf",
                          action="store_true", dest="xperf",
                          default=False,
                          help="Print xperf information")

        self.add_argument("--verbose",
                          action="store_true", dest="verbose",
                          default=False,
                          help="Output information for all tests")


def main():
    global platforms, tests
    parser = CompareOptions()
    args = parser.parse_args()

    if args.platforms:
        platforms = args.platforms

    if args.testnames:
        tests = args.testnames

    if args.masterbranch and args.masterbranch not in branches:
        parser.error(
            "ERROR: the masterbranch '%s' you specified does not exist in '%s'"
            % (args.masterbranch, branches)
        )

    if any(branch in args.branch or branch in args.masterbranch
           for branch in ['Aurora', 'Beta']) and not args.pgo:
        parser.error("Error: please specify --pgo flag in case of"
                     " Aurora/Beta branch")

    branch = None
    if args.branch in branches:
        if args.pgo:
            branch = branch_map[args.branch]['pgo']['name']
        else:
            branch = branch_map[args.branch]['nonpgo']['name']
    else:
        parser.error(
            "ERROR: the branch '%s' you specified does not exist in '%s'"
            % (args.branch, branches)
        )

    if args.skipdays:
        if args.skipdays > 30:
            parser.error("ERROR: please specify the skipdays between 0-30")

    if not args.revision:
        parser.error("ERROR: --revision is required")

    # TODO: We need to ensure we have full coverage of the pushlog before we
    # can do this.
#    alldata = getDatazillaData(args.branch)
#    datazilla, pgodatazilla, xperfdata = alldata[args.revision]
    datazilla, pgodatazilla, xperfdata = \
        getDatazillaCSET(args.revision, branch)
    if args.xperf:
        print xperfdata
    else:
        compareResults(args.revision, args.branch, args.masterbranch,
                       args.skipdays, args.history, platforms, tests,
                       args.pgo, args.printurl, args.compare_e10s, datazilla,
                       pgodatazilla, args.verbose, args.masterrevision, True)


def shorten(url):
    headers = {'content-type': 'application/json'}
    base_url = "www.googleapis.com"

    conn = httplib.HTTPSConnection(base_url)
    body = json.dumps(dict(longUrl=url))

    conn.request("POST", "/urlshortener/v1/url", body, headers)
    resp = conn.getresponse()
    if resp.reason == "OK":
        data = resp.read()
        shortened = json.loads(data)
        if 'id' in shortened:
            return shortened['id']
    return url

if __name__ == "__main__":
    main()
