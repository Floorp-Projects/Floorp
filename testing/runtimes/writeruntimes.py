from argparse import ArgumentParser
from collections import defaultdict
import json
import os
import sys

import requests

here = os.path.abspath(os.path.dirname(__file__))

ACTIVE_DATA_URL = "http://activedata.allizom.org/query"
PERCENTILE = 0.5 # ignore the bottom PERCENTILE*100% of numbers

def query_activedata(suite, platforms=None):
    platforms = ', "build.platform":%s' % json.dumps(platforms) if platforms else ''

    query = """
{
    "from":"unittest",
    "limit":200000,
    "groupby":["result.test"],
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

def write_runtimes(data, suite, indir=here, outdir=here):
    data = dict(data)

    outfilename = os.path.join(outdir, "%s.runtimes.json" % suite)
    infilename = os.path.join(indir, "%s.runtimes.json" % suite)
    if not os.path.exists(outdir):
        os.makedirs(outdir)

    # read in existing data, if any
    indata = None
    if os.path.exists(infilename):
        with open(infilename, 'r') as f:
            indata = json.loads(f.read()).get('runtimes')

    # identify a threshold of durations, below which we ignore
    runtimes = []
    for result in data.itervalues():
        duration = int(result * 1000) if result else 0
        if duration:
            runtimes.append(duration)
    runtimes.sort()
    threshold = runtimes[int(len(runtimes) * PERCENTILE)]

    # split the durations into two groups; omitted and specified
    omitted = []
    specified = indata if indata else {}
    current_tests = []
    for test, duration in data.iteritems():
        current_tests.append(test)
        duration = int(duration * 1000) if duration else 0
        if duration > 0 and duration < threshold:
            omitted.append(duration)
            if test in specified:
                del specified[test]
        elif duration >= threshold and test != "automation.py":
            original = specified.get(test, 0)
            if not original or abs(original - duration) > (original/20):
                # only write new data if it's > 20% different than original
                specified[test] = duration

    # delete any test references no longer needed
    to_delete = []
    for test in specified:
        if test not in current_tests:
            to_delete.append(test)
    for test in to_delete:
        del specified[test]

    avg = int(sum(omitted)/len(omitted))

    results = {'excluded_test_average': avg,
               'runtimes': specified}

    with open(outfilename, 'w') as f:
        f.write(json.dumps(results, indent=2, sort_keys=True))


def cli(args=sys.argv[1:]):
    parser = ArgumentParser()
    parser.add_argument('-o', '--output-directory', dest='outdir',
        default=here, help="Directory to save runtime data.")

    parser.add_argument('-i', '--input-directory', dest='indir',
        default=here, help="Directory from which to read current runtime data.")

    parser.add_argument('-p', '--platforms', default=None,
        help="Comma separated list of platforms from which to generate data.")

    parser.add_argument('-s', '--suite', dest='suite', default=None,
        help="Suite for which to generate data.")

    args = parser.parse_args(args)

    if not args.suite:
        raise ValueError("Must specify suite with the -s argument")
    if ',' in args.suite:
        raise ValueError("Passing multiple suites is not supported")

    if args.platforms:
        args.platforms = args.platforms.split(',')

    data = query_activedata(args.suite, args.platforms)
    write_runtimes(data, args.suite, indir=args.indir, outdir=args.outdir)

if __name__ == "__main__":
    sys.exit(cli())
