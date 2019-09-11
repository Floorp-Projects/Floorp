# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import collections
import itertools
import json
import math
import os
import re
import sys
import atexit
import shared_telemetry_utils as utils

from ctypes import c_int
from shared_telemetry_utils import ParserError
from collections import OrderedDict
atexit.register(ParserError.exit_func)

# Constants.
MAX_LABEL_LENGTH = 20
MAX_LABEL_COUNT = 100
MAX_KEY_COUNT = 30
MAX_KEY_LENGTH = 20
MIN_CATEGORICAL_BUCKET_COUNT = 50
CPP_IDENTIFIER_PATTERN = '^[a-z][a-z0-9_]+[a-z0-9]$'

ALWAYS_ALLOWED_KEYS = [
    'kind',
    'description',
    'operating_systems',
    'expires_in_version',
    'alert_emails',
    'keyed',
    'releaseChannelCollection',
    'bug_numbers',
    'keys',
    'record_in_processes',
    'record_into_store',
    'products',
]

BASE_DOC_URL = ("https://firefox-source-docs.mozilla.org/toolkit/components/"
                "telemetry/telemetry/")
HISTOGRAMS_DOC_URL = (BASE_DOC_URL + "collection/histograms.html")
SCALARS_DOC_URL = (BASE_DOC_URL + "collection/scalars.html")

# parse_histograms.py is used by scripts from a mozilla-central build tree
# and also by outside consumers, such as the telemetry server.  We need
# to ensure that importing things works in both contexts.  Therefore,
# unconditionally importing things that are local to the build tree, such
# as buildconfig, is a no-no.
try:
    import buildconfig

    # Need to update sys.path to be able to find usecounters.
    sys.path.append(os.path.join(buildconfig.topsrcdir, 'dom/base/'))
except ImportError:
    # Must be in an out-of-tree usage scenario.  Trust that whoever is
    # running this script knows we need the usecounters module and has
    # ensured it's in our sys.path.
    pass


def linear_buckets(dmin, dmax, n_buckets):
    ret_array = [0] * n_buckets
    dmin = float(dmin)
    dmax = float(dmax)
    for i in range(1, n_buckets):
        linear_range = (dmin * (n_buckets - 1 - i) + dmax * (i - 1)) / (n_buckets - 2)
        ret_array[i] = int(linear_range + 0.5)
    return ret_array


def exponential_buckets(dmin, dmax, n_buckets):
    log_max = math.log(dmax)
    bucket_index = 2
    ret_array = [0] * n_buckets
    current = dmin
    ret_array[1] = current
    for bucket_index in range(2, n_buckets):
        log_current = math.log(current)
        log_ratio = (log_max - log_current) / (n_buckets - bucket_index)
        log_next = log_current + log_ratio
        next_value = int(math.floor(math.exp(log_next) + 0.5))
        if next_value > current:
            current = next_value
        else:
            current = current + 1
        ret_array[bucket_index] = current
    return ret_array


allowlists = None


def load_allowlist():
    global allowlists
    try:
        parsers_path = os.path.realpath(os.path.dirname(__file__))
        # The parsers live in build_scripts/parsers in the Telemetry module, while
        # the histogram-allowlists file lives in the root of the module. Account
        # for that when looking for the allowlist.
        # NOTE: if the parsers are moved, this logic will need to be updated.
        telemetry_module_path = os.path.abspath(os.path.join(parsers_path, os.pardir, os.pardir))
        allowlist_path = os.path.join(telemetry_module_path, 'histogram-allowlists.json')
        with open(allowlist_path, 'r') as f:
            try:
                allowlists = json.load(f)
                for name, allowlist in allowlists.iteritems():
                    allowlists[name] = set(allowlist)
            except ValueError:
                ParserError('Error parsing allowlist: %s' % allowlist_path).handle_now()
    except IOError:
        allowlists = None
        ParserError('Unable to parse allowlist: %s.' % allowlist_path).handle_now()


class Histogram:
    """A class for representing a histogram definition."""

    def __init__(self, name, definition, strict_type_checks=False):
        """Initialize a histogram named name with the given definition.
definition is a dict-like object that must contain at least the keys:

 - 'kind': The kind of histogram.  Must be one of 'boolean', 'flag',
   'count', 'enumerated', 'linear', or 'exponential'.
 - 'description': A textual description of the histogram.
 - 'strict_type_checks': A boolean indicating whether to use the new, stricter type checks.
                         The server-side still has to deal with old, oddly typed submissions,
                         so we have to skip them there by default."""
        self._strict_type_checks = strict_type_checks
        self._is_use_counter = name.startswith("USE_COUNTER2_")
        if self._is_use_counter:
            definition.setdefault('record_in_processes', ['main', 'content'])
            definition.setdefault('releaseChannelCollection', 'opt-out')
            definition.setdefault('products', ['firefox', 'fennec', 'geckoview'])
        self.verify_attributes(name, definition)
        self._name = name
        self._description = definition['description']
        self._kind = definition['kind']
        self._keys = definition.get('keys', [])
        self._keyed = definition.get('keyed', False)
        self._expiration = definition.get('expires_in_version')
        self._labels = definition.get('labels', [])
        self._record_in_processes = definition.get('record_in_processes')
        self._record_into_store = definition.get('record_into_store', ['main'])
        self._products = definition.get('products')
        self._operating_systems = definition.get('operating_systems', ["all"])

        self.compute_bucket_parameters(definition)
        self.set_nsITelemetry_kind()
        self.set_dataset(definition)

    def name(self):
        """Return the name of the histogram."""
        return self._name

    def description(self):
        """Return the description of the histogram."""
        return self._description

    def kind(self):
        """Return the kind of the histogram.
Will be one of 'boolean', 'flag', 'count', 'enumerated', 'categorical', 'linear',
or 'exponential'."""
        return self._kind

    def expiration(self):
        """Return the expiration version of the histogram."""
        return self._expiration

    def nsITelemetry_kind(self):
        """Return the nsITelemetry constant corresponding to the kind of
the histogram."""
        return self._nsITelemetry_kind

    def low(self):
        """Return the lower bound of the histogram."""
        return self._low

    def high(self):
        """Return the high bound of the histogram."""
        return self._high

    def n_buckets(self):
        """Return the number of buckets in the histogram."""
        return self._n_buckets

    def keyed(self):
        """Returns True if this a keyed histogram, false otherwise."""
        return self._keyed

    def keys(self):
        """Returns a list of allowed keys for keyed histogram, [] for others."""
        return self._keys

    def dataset(self):
        """Returns the dataset this histogram belongs into."""
        return self._dataset

    def labels(self):
        """Returns a list of labels for a categorical histogram, [] for others."""
        return self._labels

    def record_in_processes(self):
        """Returns a list of processes this histogram is permitted to record in."""
        return self._record_in_processes

    def record_in_processes_enum(self):
        """Get the non-empty list of flags representing the processes to record data in"""
        return [utils.process_name_to_enum(p) for p in self.record_in_processes()]

    def products(self):
        """Get the non-empty list of products to record data on"""
        return self._products

    def products_enum(self):
        """Get the non-empty list of flags representing products to record data on"""
        return [utils.product_name_to_enum(p) for p in self.products()]

    def operating_systems(self):
        """Get the list of operating systems to record data on"""
        return self._operating_systems

    def record_on_os(self, target_os):
        """Check if this probe should be recorded on the passed os."""
        os = self.operating_systems()
        if "all" in os:
            return True

        canonical_os = utils.canonical_os(target_os)

        if "unix" in os and canonical_os in utils.UNIX_LIKE_OS:
            return True

        return canonical_os in os

    def record_into_store(self):
        """Get the non-empty list of stores to record into"""
        return self._record_into_store

    def ranges(self):
        """Return an array of lower bounds for each bucket in the histogram."""
        bucket_fns = {
            'boolean': linear_buckets,
            'flag': linear_buckets,
            'count': linear_buckets,
            'enumerated': linear_buckets,
            'categorical': linear_buckets,
            'linear': linear_buckets,
            'exponential': exponential_buckets,
        }

        if self._kind not in bucket_fns:
            ParserError('Unknown kind "%s" for histogram "%s".' %
                        (self._kind, self._name)).handle_later()

        fn = bucket_fns[self._kind]
        return fn(self.low(), self.high(), self.n_buckets())

    def compute_bucket_parameters(self, definition):
        bucket_fns = {
            'boolean': Histogram.boolean_flag_bucket_parameters,
            'flag': Histogram.boolean_flag_bucket_parameters,
            'count': Histogram.boolean_flag_bucket_parameters,
            'enumerated': Histogram.enumerated_bucket_parameters,
            'categorical': Histogram.categorical_bucket_parameters,
            'linear': Histogram.linear_bucket_parameters,
            'exponential': Histogram.exponential_bucket_parameters,
        }

        if self._kind not in bucket_fns:
            ParserError('Unknown kind "%s" for histogram "%s".' %
                        (self._kind, self._name)).handle_later()

        fn = bucket_fns[self._kind]
        self.set_bucket_parameters(*fn(definition))

    def verify_attributes(self, name, definition):
        global ALWAYS_ALLOWED_KEYS
        general_keys = ALWAYS_ALLOWED_KEYS + ['low', 'high', 'n_buckets']

        table = {
            'boolean': ALWAYS_ALLOWED_KEYS,
            'flag': ALWAYS_ALLOWED_KEYS,
            'count': ALWAYS_ALLOWED_KEYS,
            'enumerated': ALWAYS_ALLOWED_KEYS + ['n_values'],
            'categorical': ALWAYS_ALLOWED_KEYS + ['labels', 'n_values'],
            'linear': general_keys,
            'exponential': general_keys,
        }
        # We removed extended_statistics_ok on the client, but the server-side,
        # where _strict_type_checks==False, has to deal with historical data.
        if not self._strict_type_checks:
            table['exponential'].append('extended_statistics_ok')

        kind = definition['kind']
        if kind not in table:
            ParserError('Unknown kind "%s" for histogram "%s".' % (kind, name)).handle_later()
        allowed_keys = table[kind]

        self.check_name(name)
        self.check_keys(name, definition, allowed_keys)
        self.check_keys_field(name, definition)
        self.check_field_types(name, definition)
        self.check_allowlisted_kind(name, definition)
        self.check_allowlistable_fields(name, definition)
        self.check_expiration(name, definition)
        self.check_label_values(name, definition)
        self.check_record_in_processes(name, definition)
        self.check_products(name, definition)
        self.check_operating_systems(name, definition)
        self.check_record_into_store(name, definition)

    def check_name(self, name):
        if '#' in name:
            ParserError('Error for histogram name "%s": "#" is not allowed.' %
                        (name)).handle_later()

        # Avoid C++ identifier conflicts between histogram enums and label enum names.
        if name.startswith("LABELS_"):
            ParserError('Error for histogram name "%s":  can not start with "LABELS_".' %
                        (name)).handle_later()

        # To make it easier to generate C++ identifiers from this etc., we restrict
        # the histogram names to a strict pattern.
        # We skip this on the server to avoid failures with old Histogram.json revisions.
        if self._strict_type_checks:
            if not re.match(CPP_IDENTIFIER_PATTERN, name, re.IGNORECASE):
                ParserError('Error for histogram name "%s": name does not conform to "%s"' %
                            (name, CPP_IDENTIFIER_PATTERN)).handle_later()

    def check_expiration(self, name, definition):
        field = 'expires_in_version'
        expiration = definition.get(field)

        if not expiration:
            return

        # We forbid new probes from using "expires_in_version" : "default" field/value pair.
        # Old ones that use this are added to the allowlist.
        if expiration == "default" and \
           allowlists is not None and \
           name not in allowlists['expiry_default']:
            ParserError('New histogram "%s" cannot have "default" %s value.' %
                        (name, field)).handle_later()

        # Historical editions of Histograms.json can have the deprecated
        # expiration format 'N.Na1'. Fortunately, those scripts set
        # self._strict_type_checks to false.
        if expiration != "default" and \
           not utils.validate_expiration_version(expiration) and \
           self._strict_type_checks:
            ParserError(('Error for histogram {} - invalid {}: {}.'
                         '\nSee: {}#expires-in-version')
                        .format(name, field, expiration, HISTOGRAMS_DOC_URL)).handle_later()

        expiration = utils.add_expiration_postfix(expiration)

        definition[field] = expiration

    def check_label_values(self, name, definition):
        labels = definition.get('labels')
        if not labels:
            return

        invalid = filter(lambda l: len(l) > MAX_LABEL_LENGTH, labels)
        if len(invalid) > 0:
            ParserError('Label values for "%s" exceed length limit of %d: %s' %
                        (name, MAX_LABEL_LENGTH, ', '.join(invalid))).handle_later()

        if len(labels) > MAX_LABEL_COUNT:
            ParserError('Label count for "%s" exceeds limit of %d' %
                        (name, MAX_LABEL_COUNT)).handle_now()

        # To make it easier to generate C++ identifiers from this etc., we restrict
        # the label values to a strict pattern.
        invalid = filter(lambda l: not re.match(CPP_IDENTIFIER_PATTERN, l, re.IGNORECASE), labels)
        if len(invalid) > 0:
            ParserError('Label values for %s are not matching pattern "%s": %s' %
                        (name, CPP_IDENTIFIER_PATTERN, ', '.join(invalid))).handle_later()

    def check_record_in_processes(self, name, definition):
        if not self._strict_type_checks:
            return

        field = 'record_in_processes'
        rip = definition.get(field)

        DOC_URL = HISTOGRAMS_DOC_URL + "#record-in-processes"

        if not rip:
            ParserError('Histogram "%s" must have a "%s" field:\n%s'
                        % (name, field, DOC_URL)).handle_later()

        for process in rip:
            if not utils.is_valid_process_name(process):
                ParserError('Histogram "%s" has unknown process "%s" in %s.\n%s' %
                            (name, process, field, DOC_URL)).handle_later()

    def check_products(self, name, definition):
        if not self._strict_type_checks:
            return

        field = 'products'
        products = definition.get(field)

        DOC_URL = HISTOGRAMS_DOC_URL + "#products"

        if not products:
            ParserError('Histogram "%s" must have a "%s" field:\n%s'
                        % (name, field, DOC_URL)).handle_now()

        for product in products:
            if not utils.is_valid_product(product):
                ParserError('Histogram "%s" has unknown product "%s" in %s.\n%s' %
                            (name, product, field, DOC_URL)).handle_later()

    def check_operating_systems(self, name, definition):
        if not self._strict_type_checks:
            return

        field = 'operating_systems'
        operating_systems = definition.get(field)

        DOC_URL = HISTOGRAMS_DOC_URL + "#operating-systems"

        if not operating_systems:
            # operating_systems is optional
            return

        for operating_system in operating_systems:
            if not utils.is_valid_os(operating_system):
                ParserError('Histogram "%s" has unknown operating system "%s" in %s.\n%s' %
                            (name, operating_system, field, DOC_URL)).handle_later()

    def check_record_into_store(self, name, definition):
        if not self._strict_type_checks:
            return

        field = 'record_into_store'
        DOC_URL = HISTOGRAMS_DOC_URL + "#record-into-store"

        if field not in definition:
            # record_into_store is optional
            return

        record_into_store = definition.get(field)
        # record_into_store should not be empty
        if not record_into_store:
            ParserError('Histogram "%s" has empty list of stores, which is not allowed.\n%s' %
                        (name, DOC_URL)).handle_later()

    def check_keys_field(self, name, definition):
        keys = definition.get('keys')
        if not self._strict_type_checks or keys is None:
            return

        if not definition.get('keyed', False):
            raise ValueError("'keys' field is not valid for %s; only allowed for keyed histograms."
                             % (name))

        if len(keys) == 0:
            raise ValueError('The key list for %s cannot be empty' % (name))

        if len(keys) > MAX_KEY_COUNT:
            raise ValueError('Label count for %s exceeds limit of %d' % (name, MAX_KEY_COUNT))

        invalid = filter(lambda k: len(k) > MAX_KEY_LENGTH, keys)
        if len(invalid) > 0:
            raise ValueError('"keys" values for %s are exceeding length "%d": %s' %
                             (name, MAX_KEY_LENGTH, ', '.join(invalid)))

    def check_allowlisted_kind(self, name, definition):
        # We don't need to run any of these checks on the server.
        if not self._strict_type_checks or allowlists is None:
            return

        # Disallow "flag" and "count" histograms on desktop, suggest to use
        # scalars instead. Allow using these histograms on Android, as we
        # don't support scalars there yet.
        hist_kind = definition.get("kind")
        android_target = "android" in definition.get("operating_systems", [])

        if not android_target and \
           hist_kind in ["flag", "count"] and \
           name not in allowlists["kind"]:
            ParserError(('Unsupported kind "%s" for histogram "%s":\n'
                         'New "%s" histograms are not supported on Desktop, you should'
                         ' use scalars instead:\n'
                         '%s\n'
                         'Are you trying to add a histogram on Android?'
                         ' Add "operating_systems": ["android"] to your histogram definition.')
                        % (hist_kind, name, hist_kind, SCALARS_DOC_URL)).handle_now()

    # Check for the presence of fields that old histograms are allowlisted for.
    def check_allowlistable_fields(self, name, definition):
        # Use counters don't have any mechanism to add the fields checked here,
        # so skip the check for them.
        # We also don't need to run any of these checks on the server.
        if self._is_use_counter or not self._strict_type_checks:
            return

        # In the pipeline we don't have allowlists available.
        if allowlists is None:
            return

        for field in ['alert_emails', 'bug_numbers']:
            if field not in definition and name not in allowlists[field]:
                ParserError('New histogram "%s" must have a "%s" field.' %
                            (name, field)).handle_later()
            if field in definition and name in allowlists[field]:
                msg = 'Histogram "%s" should be removed from the allowlist for "%s" in ' \
                      'histogram-allowlists.json.'
                ParserError(msg % (name, field)).handle_later()

    def check_field_types(self, name, definition):
        # Define expected types for the histogram properties.
        type_checked_fields = {
            "n_buckets": int,
            "n_values": int,
            "low": int,
            "high": int,
            "keyed": bool,
            "expires_in_version": basestring,
            "kind": basestring,
            "description": basestring,
            "releaseChannelCollection": basestring,
        }

        # For list fields we check the items types.
        type_checked_list_fields = {
            "bug_numbers": int,
            "alert_emails": basestring,
            "labels": basestring,
            "record_in_processes": basestring,
            "keys": basestring,
            "products": basestring,
            "operating_systems": basestring,
            "record_into_store": basestring,
        }

        # For the server-side, where _strict_type_checks==False, we want to
        # skip the stricter type checks for these fields for dealing with
        # historical data.
        coerce_fields = ["low", "high", "n_values", "n_buckets"]
        if not self._strict_type_checks:
            # This handles some old non-numeric expressions.
            EXPRESSIONS = {
                "JS::GCReason::NUM_TELEMETRY_REASONS": 101,
                "mozilla::StartupTimeline::MAX_EVENT_ID": 12,
            }

            def try_to_coerce_to_number(v):
                if v in EXPRESSIONS:
                    return EXPRESSIONS[v]
                try:
                    return eval(v, {})
                except Exception:
                    return v
            for key in [k for k in coerce_fields if k in definition]:
                definition[key] = try_to_coerce_to_number(definition[key])
            # This handles old "keyed":"true" definitions (bug 1271986).
            if definition.get("keyed", None) == "true":
                definition["keyed"] = True

        def nice_type_name(t):
            if t is basestring:
                return "string"
            return t.__name__

        for key, key_type in type_checked_fields.iteritems():
            if key not in definition:
                continue
            if not isinstance(definition[key], key_type):
                ParserError('Value for key "{0}" in histogram "{1}" should be {2}.'
                            .format(key, name, nice_type_name(key_type))).handle_later()

        # Make sure the max range is lower than or equal to INT_MAX
        if "high" in definition and not c_int(definition["high"]).value > 0:
            ParserError('Value for high in histogram "{0}" should be lower or equal to INT_MAX.'
                        .format(nice_type_name(c_int))).handle_later()

        for key, key_type in type_checked_list_fields.iteritems():
            if key not in definition:
                continue
            if not all(isinstance(x, key_type) for x in definition[key]):
                ParserError('All values for list "{0}" in histogram "{1}" should be of type'
                            ' {2}.'.format(key, name, nice_type_name(key_type))).handle_later()

    def check_keys(self, name, definition, allowed_keys):
        if not self._strict_type_checks:
            return
        for key in definition.iterkeys():
            if key not in allowed_keys:
                ParserError('Key "%s" is not allowed for histogram "%s".' %
                            (key, name)).handle_later()

    def set_bucket_parameters(self, low, high, n_buckets):
        self._low = low
        self._high = high
        self._n_buckets = n_buckets
        if allowlists is not None and self._n_buckets > 100 and type(self._n_buckets) is int:
            if self._name not in allowlists['n_buckets']:
                ParserError(
                    'New histogram "%s" is not permitted to have more than 100 buckets.\n'
                    'Histograms with large numbers of buckets use disproportionately high'
                    ' amounts of resources. Contact a Telemetry peer (e.g. in #telemetry)'
                    ' if you think an exception ought to be made:\n'
                    'https://wiki.mozilla.org/Modules/Toolkit#Telemetry'
                    % self._name
                    ).handle_later()

    @staticmethod
    def boolean_flag_bucket_parameters(definition):
        return (1, 2, 3)

    @staticmethod
    def linear_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

    @staticmethod
    def enumerated_bucket_parameters(definition):
        n_values = definition['n_values']
        return (1, n_values, n_values + 1)

    @staticmethod
    def categorical_bucket_parameters(definition):
        # Categorical histograms default to 50 buckets to make working with them easier.
        # Otherwise when adding labels later we run into problems with the pipeline not
        # supporting bucket changes.
        # This can be overridden using the n_values field.
        n_values = max(len(definition['labels']),
                       definition.get('n_values', 0),
                       MIN_CATEGORICAL_BUCKET_COUNT)
        return (1, n_values, n_values + 1)

    @staticmethod
    def exponential_bucket_parameters(definition):
        return (definition.get('low', 1),
                definition['high'],
                definition['n_buckets'])

    def set_nsITelemetry_kind(self):
        # Pick a Telemetry implementation type.
        types = {
            'boolean': 'BOOLEAN',
            'flag': 'FLAG',
            'count': 'COUNT',
            'enumerated': 'LINEAR',
            'categorical': 'CATEGORICAL',
            'linear': 'LINEAR',
            'exponential': 'EXPONENTIAL',
        }

        if self._kind not in types:
            ParserError('Unknown kind "%s" for histogram "%s".' %
                        (self._kind, self._name)).handle_later()

        self._nsITelemetry_kind = "nsITelemetry::HISTOGRAM_%s" % types[self._kind]

    def set_dataset(self, definition):
        datasets = {
            'opt-in': 'DATASET_PRERELEASE_CHANNELS',
            'opt-out': 'DATASET_ALL_CHANNELS'
        }

        value = definition.get('releaseChannelCollection', 'opt-in')
        if value not in datasets:
            ParserError('Unknown value for releaseChannelCollection'
                        ' policy for histogram "%s".' % self._name).handle_later()

        self._dataset = "nsITelemetry::" + datasets[value]


# This hook function loads the histograms into an OrderedDict.
# It will raise a ParserError if duplicate keys are found.
def load_histograms_into_dict(ordered_pairs, strict_type_checks):
    d = collections.OrderedDict()
    for key, value in ordered_pairs:
        if strict_type_checks and key in d:
            ParserError("Found duplicate key in Histograms file: %s" % key).handle_later()
        d[key] = value
    return d


# We support generating histograms from multiple different input files, not
# just Histograms.json.  For each file's basename, we have a specific
# routine to parse that file, and return a dictionary mapping histogram
# names to histogram parameters.
def from_Histograms_json(filename, strict_type_checks):
    with open(filename, 'r') as f:
        try:
            def hook(ps):
                return load_histograms_into_dict(ps, strict_type_checks)
            histograms = json.load(f, object_pairs_hook=hook)
        except ValueError as e:
            ParserError("error parsing histograms in %s: %s" % (filename, e.message)).handle_now()
    return histograms


def from_UseCounters_conf(filename, strict_type_checks):
    return usecounters.generate_histograms(filename)


def from_nsDeprecatedOperationList(filename, strict_type_checks):
    operation_regex = re.compile('^DEPRECATED_OPERATION\\(([^)]+)\\)')
    histograms = collections.OrderedDict()

    with open(filename, 'r') as f:
        for line in f:
            match = operation_regex.search(line)
            if not match:
                continue

            op = match.group(1)

            def add_counter(context):
                name = 'USE_COUNTER2_DEPRECATED_%s_%s' % (op, context.upper())
                histograms[name] = {
                    'expires_in_version': 'never',
                    'kind': 'boolean',
                    'description': 'Whether a %s used %s' % (context, op)
                }
            add_counter('document')
            add_counter('page')

    return histograms


FILENAME_PARSERS = {
    'Histograms.json': from_Histograms_json,
    'nsDeprecatedOperationList.h': from_nsDeprecatedOperationList,
}

# Similarly to the dance above with buildconfig, usecounters may not be
# available, so handle that gracefully.
try:
    import usecounters

    FILENAME_PARSERS['UseCounters.conf'] = from_UseCounters_conf
except ImportError:
    pass


def from_files(filenames, strict_type_checks=True):
    """Return an iterator that provides a sequence of Histograms for
the histograms defined in filenames.
    """
    if strict_type_checks:
        load_allowlist()

    all_histograms = OrderedDict()
    for filename in filenames:
        parser = FILENAME_PARSERS[os.path.basename(filename)]
        histograms = parser(filename, strict_type_checks)

        # OrderedDicts are important, because then the iteration order over
        # the parsed histograms is stable, which makes the insertion into
        # all_histograms stable, which makes ordering in generated files
        # stable, which makes builds more deterministic.
        if not isinstance(histograms, OrderedDict):
            ParserError("Histogram parser did not provide an OrderedDict.").handle_now()

        for (name, definition) in histograms.iteritems():
            if name in all_histograms:
                ParserError('Duplicate histogram name "%s".' % name).handle_later()
            all_histograms[name] = definition

    # We require that all USE_COUNTER2_* histograms be defined in a contiguous
    # block.
    use_counter_indices = filter(lambda x: x[1].startswith("USE_COUNTER2_"),
                                 enumerate(all_histograms.iterkeys()))
    if use_counter_indices:
        lower_bound = use_counter_indices[0][0]
        upper_bound = use_counter_indices[-1][0]
        n_counters = upper_bound - lower_bound + 1
        if n_counters != len(use_counter_indices):
            ParserError("Use counter histograms must be defined in a contiguous block."
                        ).handle_later()

    # Check that histograms that were removed from Histograms.json etc.
    # are also removed from the allowlists.
    if allowlists is not None:
        all_allowlist_entries = itertools.chain.from_iterable(allowlists.itervalues())
        orphaned = set(all_allowlist_entries) - set(all_histograms.keys())
        if len(orphaned) > 0:
            msg = 'The following entries are orphaned and should be removed from ' \
                  'histogram-allowlists.json:\n%s'
            ParserError(msg % (', '.join(sorted(orphaned)))).handle_later()

    for (name, definition) in all_histograms.iteritems():
        yield Histogram(name, definition, strict_type_checks=strict_type_checks)
