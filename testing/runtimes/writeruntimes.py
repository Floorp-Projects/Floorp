from argparse import ArgumentParser
from collections import defaultdict
import json
import os
import sys

import requests

here = os.path.abspath(os.path.dirname(__file__))

ACTIVE_DATA_URL = "http://activedata.allizom.org/query"
PERCENTILE = 0.9 # ignore the bottom PERCENTILE*100% of numbers

def query_activedata(suite, platforms=None):
    platforms = ', "build.platform":%s' % json.dumps(platforms) if platforms else ''

    query = """
{
    "from":"unittest",
    "limit":200000,
    "groupby":["build.platform","result.test","build.type"],
    "select":{"value":"result.duration","aggregate":"average"},
    "where":{"and":[
        {"eq":{"suite":"%s"%s}},
        {"gt":{"run.timestamp":"{{today-week}}"}}
    ]}
}
""" % (suite, platforms)

    response = requests.post(ACTIVE_DATA_URL,
                             data=query,
                             stream=True)
    response.raise_for_status()
    data = response.json()["data"]
    return data

def write_runtimes(data, suite):
    testdata = defaultdict(lambda: defaultdict(list))
    for result in data:
        # Each result is a list with four members: platform,
        # test, build type, and duration.
        platform = result[0]
        buildtype = result[2]
        testdata[platform][buildtype].append([result[1], result[3]])

    for platform in testdata:
        for buildtype in testdata[platform]:
            print 'processing %s-%s results' % (platform, buildtype)

            # identify a threshold of durations, below which we ignore
            runtimes = []
            for result in testdata[platform][buildtype]:
                duration = int(result[1] * 1000) if result[1] else 0
                if duration:
                    runtimes.append(duration)
            runtimes.sort()
            threshold = runtimes[int(len(runtimes) * PERCENTILE)]

            # split the durations into two groups; excluded and specified
            ommitted = []
            specified = {}
            for test, duration in testdata[platform][buildtype]:
                duration = int(duration * 1000) if duration else 0
                if duration > 0 and duration < threshold:
                    ommitted.append(duration)
                elif duration >= threshold:
                    specified[test] = duration
            avg = int(sum(ommitted)/len(ommitted))

            results = {'excluded_test_average': avg,
                       'runtimes': specified}

            dirname = "%s-%s" % (platform, buildtype)
            if not os.path.exists(dirname):
                os.mkdir(dirname)
            outfilename = os.path.join(dirname, "%s.runtimes.json" % suite)
            with open(outfilename, 'w') as f:
                f.write(json.dumps(results))


def cli(args=sys.argv[1:]):
    parser = ArgumentParser()
    parser.add_argument('-o', '--output-directory', dest='outdir',
        default=here, help="Directory to save runtime data.")

    parser.add_argument('-p', '--platforms', default=None,
        help="Comma separated list of platforms from which to generate data.")

    parser.add_argument('-s', '--suite', dest='suite', default=None,
        help="Suite for which to generate data.")

    args = parser.parse_args(args)

    if not args.suite:
        raise ValueError("Must specify suite with the -u argument")
    if ',' in args.suite:
        raise ValueError("Passing multiple suites is not supported")

    if args.platforms:
        args.platforms = args.platforms.split(',')

    data = query_activedata(args.suite, args.platforms)
    write_runtimes(data, args.suite)

if __name__ == "__main__":
    sys.exit(cli())
