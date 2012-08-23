# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement

import simplejson as json

def table_dispatch(kind, table, body):
    """Call body with table[kind] if it exists.  Raise an error otherwise."""
    if kind in table:
        body(table[kind])
    else:
        raise BaseException, "don't know how to handle a histogram of kind %s" % kind

always_allowed_keys = ['kind', 'description', 'cpp_guard']

class Histogram:
    """A class for representing a histogram definition."""

    def __init__(self, name, definition):
        """Initialize a histogram named name with the given definition.
definition is a dict-like object that must contain at least the keys:

 - 'kind': The kind of histogram.  Must be one of 'boolean', 'flag',
   'enumerated', 'linear', or 'exponential'.
 - 'description': A textual description of the histogram.

The key 'cpp_guard' is optional; if present, it denotes a preprocessor
symbol that should guard C/C++ definitions associated with the histogram."""
        self.verify_attributes(name, definition)
        self._name = name
        self._description = definition['description']
        self._kind = definition['kind']
        self._cpp_guard = definition.get('cpp_guard')
        self.compute_bucket_parameters(definition)
        table = { 'boolean': 'BOOLEAN',
                  'flag': 'FLAG',
                  'enumerated': 'LINEAR',
                  'linear': 'LINEAR',
                  'exponential': 'EXPONENTIAL' }
        table_dispatch(self.kind(), table,
                       lambda k: self._set_nsITelemetry_kind(k))

    def name(self):
        """Return the name of the histogram."""
        return self._name

    def description(self):
        """Return the description of the histogram."""
        return self._description

    def kind(self):
        """Return the kind of the histogram.
Will be one of 'boolean', 'flag', 'enumerated', 'linear', or 'exponential'."""
        return self._kind

    def nsITelemetry_kind(self):
        """Return the nsITelemetry constant corresponding to the kind of
the histogram."""
        return self._nsITelemetry_kind

    def _set_nsITelemetry_kind(self, kind):
        self._nsITelemetry_kind = "nsITelemetry::HISTOGRAM_%s" % kind

    def low(self):
        """Return the lower bound of the histogram.  May be a string."""
        return self._low

    def high(self):
        """Return the high bound of the histogram.  May be a string."""
        return self._high

    def n_buckets(self):
        """Return the number of buckets in the histogram.  May be a string."""
        return self._n_buckets

    def cpp_guard(self):
        """Return the preprocessor symbol that should guard C/C++ definitions
associated with the histogram.  Returns None if no guarding is necessary."""
        return self._cpp_guard

    def compute_bucket_parameters(self, definition):
        table = {
            'boolean': Histogram.boolean_flag_bucket_parameters,
            'flag': Histogram.boolean_flag_bucket_parameters,
            'enumerated': Histogram.enumerated_bucket_parameters,
            'linear': Histogram.linear_bucket_parameters,
            'exponential': Histogram.exponential_bucket_parameters
            }
        table_dispatch(self.kind(), table,
                       lambda p: self.set_bucket_parameters(*p(definition)))

    def verify_attributes(self, name, definition):
        global always_allowed_keys
        general_keys = always_allowed_keys + ['low', 'high', 'n_buckets']

        table = {
            'boolean': always_allowed_keys,
            'flag': always_allowed_keys,
            'enumerated': always_allowed_keys + ['n_values'],
            'linear': general_keys,
            'exponential': general_keys
            }
        table_dispatch(definition['kind'], table,
                       lambda allowed_keys: Histogram.check_keys(name, definition, allowed_keys))

    @staticmethod
    def check_keys(name, definition, allowed_keys):
        for key in definition.iterkeys():
            if key not in allowed_keys:
                raise KeyError, '%s not permitted for %s' % (key, name)

    def set_bucket_parameters(self, low, high, n_buckets):
        (self._low, self._high, self._n_buckets) = (low, high, n_buckets)

    @staticmethod
    def boolean_flag_bucket_parameters(definition):
        return (0, 1, 2)

    @staticmethod
    def linear_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

    @staticmethod
    def enumerated_bucket_parameters(definition):
        n_values = definition['n_values']
        return (1, n_values, "%s+1" % n_values)

    @staticmethod
    def exponential_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

def from_file(filename):
    """Return an iterator that provides a sequence of Histograms for
the histograms defined in filename.
    """
    with open(filename, 'r') as f:
        histograms = json.load(f, object_pairs_hook=json.OrderedDict)
        for (name, definition) in histograms.iteritems():
            yield Histogram(name, definition)
