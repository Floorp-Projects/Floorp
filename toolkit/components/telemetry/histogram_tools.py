# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement

import simplejson as json

def from_file(filename):
    """Return an iterator that provides (name, definition) tuples of
histograms defined in filename.
    """
    with open(filename, 'r') as f:
        histograms = json.load(f, object_pairs_hook=json.OrderedDict)
        return histograms.iteritems()
