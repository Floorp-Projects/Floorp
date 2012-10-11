#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out detailed histogram information, including the ranges of the
# buckets specified by each histogram.

import sys
import re
import histogram_tools
import json

# For compatibility with python 2.6
try:
    from collections import OrderedDict
except ImportError:
    from simplejson import OrderedDict

# Keep this in sync with TelemetryPing.
startup_histogram_re = re.compile("SQLITE|HTTP|SPDY|CACHE|DNS")

def main(argv):
    filename = argv[0]
    all_histograms = OrderedDict()

    for histogram in histogram_tools.from_file(filename):
        name = histogram.name()
        parameters = OrderedDict()
        table = {
            'boolean': '2',
            'flag': '3',
            'enumerated': '1',
            'linear': '1',
            'exponential': '0'
            }
        # Use __setitem__ because Python lambdas are so limited.
        histogram_tools.table_dispatch(histogram.kind(), table,
                                       lambda k: parameters.__setitem__('kind', k))
        if histogram.low() == 0:
            parameters['min'] = 1
        else:
            parameters['min'] = histogram.low()

        try:
            buckets = histogram.ranges()
            parameters['buckets'] = buckets
            parameters['max'] = buckets[-1]
            parameters['bucket_count'] = len(buckets)
        except histogram_tools.DefinitionException:
            continue

        all_histograms.update({ name: parameters });

        if startup_histogram_re.search(name) is not None:
            all_histograms.update({ "STARTUP_" + name: parameters })

    print json.dumps({ 'histograms': all_histograms})

main(sys.argv[1:])
