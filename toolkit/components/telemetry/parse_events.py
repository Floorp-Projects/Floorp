# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import yaml
import itertools
import datetime
import string
import shared_telemetry_utils as utils

MAX_CATEGORY_NAME_LENGTH = 30
MAX_METHOD_NAME_LENGTH = 20
MAX_OBJECT_NAME_LENGTH = 20
MAX_EXTRA_KEYS_COUNT = 10
MAX_EXTRA_KEY_NAME_LENGTH = 15

IDENTIFIER_PATTERN = r'^[a-zA-Z][a-zA-Z0-9_.]+[a-zA-Z0-9]$'
DATE_PATTERN = r'^[0-9]{4}-[0-9]{2}-[0-9]{2}$'


def nice_type_name(t):
    if isinstance(t, basestring):
        return "string"
    return t.__name__


def convert_to_cpp_identifier(s, sep):
    return string.capwords(s, sep).replace(sep, "")


class OneOf:
    """This is a placeholder type for the TypeChecker below.
    It signals that the checked value should match one of the following arguments
    passed to the TypeChecker constructor.
    """
    pass


class TypeChecker:
    """This implements a convenience type TypeChecker to make the validation code more readable."""
    def __init__(self, kind, *args):
        """This takes 1-3 arguments, specifying the value type to check for.
        It supports:
        - atomic values, e.g.: TypeChecker(int)
        - list values, e.g.: TypeChecker(list, basestring)
        - dict values, e.g.: TypeChecker(dict, basestring, int)
        - atomic values that can have different types, e.g.: TypeChecker(OneOf, int, date)"""
        self._kind = kind
        self._args = args

    def check(self, identifier, key, value):
        # Check fields that can be one of two different types.
        if self._kind is OneOf:
            if not isinstance(value, self._args[0]) and not isinstance(value, self._args[1]):
                raise ValueError, "%s: failed type check for %s - expected %s or %s, got %s" %\
                                  (identifier, key,
                                   nice_type_name(self._args[0]),
                                   nice_type_name(self._args[1]),
                                   nice_type_name(type(value)))
            return

        # Check basic type of value.
        if not isinstance(value, self._kind):
            raise ValueError, "%s: failed type check for %s - expected %s, got %s" %\
                              (identifier, key,
                               nice_type_name(self._kind),
                               nice_type_name(type(value)))

        # Check types of values in lists.
        if self._kind is list:
            if len(value) < 1:
                raise ValueError, "%s: failed check for %s - list should not be empty" % (identifier, key)
            for x in value:
                if not isinstance(x, self._args[0]):
                    raise ValueError, "%s: failed type check for %s - expected list value type %s, got %s" %\
                                      (identifier, key,
                                       nice_type_name(self._args[0]),
                                       nice_type_name(type(x)))

        # Check types of keys and values in dictionaries.
        elif self._kind is dict:
            if len(value.keys()) < 1:
                    raise ValueError, "%s: failed check for %s - dict should not be empty" % (identifier, key)
            for x in value.iterkeys():
                if not isinstance(x, self._args[0]):
                    raise ValueError, "%s: failed dict type check for %s - expected key type %s, got %s" %\
                                      (identifier, key,
                                       nice_type_name(self._args[0]),
                                       nice_type_name(type(x)))
            for k, v in value.iteritems():
                if not isinstance(x, self._args[1]):
                    raise ValueError, "%s: failed dict type check for %s - expected value type %s for key %s, got %s" %\
                                      (identifier, key,
                                       nice_type_name(self._args[1]),
                                       k,
                                       nice_type_name(type(x)))


def type_check_event_fields(identifier, name, definition):
    """Perform a type/schema check on the event definition."""
    REQUIRED_FIELDS = {
        'objects': TypeChecker(list, basestring),
        'bug_numbers': TypeChecker(list, int),
        'notification_emails': TypeChecker(list, basestring),
        'record_in_processes': TypeChecker(list, basestring),
        'description': TypeChecker(basestring),
    }
    OPTIONAL_FIELDS = {
        'methods': TypeChecker(list, basestring),
        'release_channel_collection': TypeChecker(basestring),
        'expiry_date': TypeChecker(OneOf, basestring, datetime.date),
        'expiry_version': TypeChecker(basestring),
        'extra_keys': TypeChecker(dict, basestring, basestring),
    }
    ALL_FIELDS = REQUIRED_FIELDS.copy()
    ALL_FIELDS.update(OPTIONAL_FIELDS)

    # Check that all the required fields are available.
    missing_fields = [f for f in REQUIRED_FIELDS.keys() if f not in definition]
    if len(missing_fields) > 0:
        raise KeyError(identifier + ' - missing required fields: ' + ', '.join(missing_fields))

    # Is there any unknown field?
    unknown_fields = [f for f in definition.keys() if f not in ALL_FIELDS]
    if len(unknown_fields) > 0:
        raise KeyError(identifier + ' - unknown fields: ' + ', '.join(unknown_fields))

    # Type-check fields.
    for k, v in definition.iteritems():
        ALL_FIELDS[k].check(identifier, k, v)


def string_check(identifier, field, value, min_length=1, max_length=None, regex=None):
    # Length check.
    if len(value) < min_length:
        raise ValueError("%s: value '%s' for field %s is less than minimum length of %d" %
                         (identifier, value, field, min_length))
    if max_length and len(value) > max_length:
        raise ValueError("%s: value '%s' for field %s is greater than maximum length of %d" %
                         (identifier, value, field, max_length))
    # Regex check.
    if regex and not re.match(regex, value):
        raise ValueError, '%s: string value "%s" for %s is not matching pattern "%s"' % \
                          (identifier, value, field, regex)


class EventData:
    """A class representing one event."""

    def __init__(self, category, name, definition):
        self._category = category
        self._name = name
        self._definition = definition

        type_check_event_fields(self.identifier, name, definition)

        # Check method & object string patterns.
        for method in self.methods:
            string_check(self.identifier, field='methods', value=method,
                         min_length=1, max_length=MAX_METHOD_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)
        for obj in self.objects:
            string_check(self.identifier, field='objects', value=obj,
                         min_length=1, max_length=MAX_OBJECT_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)

        # Check release_channel_collection
        rcc_key = 'release_channel_collection'
        rcc = definition.get(rcc_key, 'opt-in')
        allowed_rcc = ["opt-in", "opt-out"]
        if not rcc in allowed_rcc:
            raise ValueError, "%s: value for %s should be one of: %s" %\
                              (self.identifier, rcc_key, ", ".join(allowed_rcc))

        # Check record_in_processes.
        record_in_processes = definition.get('record_in_processes')
        for proc in record_in_processes:
            if not utils.is_valid_process_name(proc):
                raise ValueError(self.identifier + ': unknown value in record_in_processes: ' + proc)

        # Check extra_keys.
        extra_keys = definition.get('extra_keys', {})
        if len(extra_keys.keys()) > MAX_EXTRA_KEYS_COUNT:
            raise ValueError, "%s: number of extra_keys exceeds limit %d" %\
                              (self.identifier, MAX_EXTRA_KEYS_COUNT)
        for key in extra_keys.iterkeys():
            string_check(self.identifier, field='extra_keys', value=key,
                         min_length=1, max_length=MAX_EXTRA_KEY_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)

        # Check expiry.
        if not 'expiry_version' in definition and not 'expiry_date' in definition:
            raise KeyError, "%s: event is missing an expiration - either expiry_version or expiry_date is required" %\
                            (self.identifier)
        expiry_date = definition.get('expiry_date')
        if expiry_date and isinstance(expiry_date, basestring) and expiry_date != 'never':
            if not re.match(DATE_PATTERN, expiry_date):
                raise ValueError, "%s: event has invalid expiry_date, it should be either 'never' or match this format: %s" %\
                                  (self.identifier, DATE_PATTERN)
            # Parse into date.
            definition['expiry_date'] = datetime.datetime.strptime(expiry_date, '%Y-%m-%d')

        # Finish setup.
        definition['expiry_version'] = utils.add_expiration_postfix(definition.get('expiry_version', 'never'))

    @property
    def category(self):
        return self._category

    @property
    def category_cpp(self):
        # Transform e.g. category.example into CategoryExample.
        return convert_to_cpp_identifier(self._category, ".")

    @property
    def name(self):
        return self._name

    @property
    def identifier(self):
        return self.category + "#" + self.name

    @property
    def methods(self):
        return self._definition.get('methods', [self.name])

    @property
    def objects(self):
        return self._definition.get('objects')

    @property
    def record_in_processes(self):
        return self._definition.get('record_in_processes')

    @property
    def record_in_processes_enum(self):
        """Get the non-empty list of flags representing the processes to record data in"""
        return [utils.process_name_to_enum(p) for p in self.record_in_processes]

    @property
    def expiry_version(self):
        return self._definition.get('expiry_version')

    @property
    def expiry_day(self):
        date = self._definition.get('expiry_date')
        if not date:
            return 0
        if isinstance(date, basestring) and date == 'never':
            return 0

        # Convert date to days since UNIX epoch.
        epoch = datetime.date(1970, 1, 1)
        days = (date - epoch).total_seconds() / (24 * 60 * 60)
        return round(days)

    @property
    def cpp_guard(self):
        return self._definition.get('cpp_guard')

    @property
    def enum_labels(self):
        def enum(method_name, object_name):
            m = convert_to_cpp_identifier(method_name, "_")
            o = convert_to_cpp_identifier(object_name, "_")
            return m + '_' + o
        combinations = itertools.product(self.methods, self.objects)
        return [enum(t[0], t[1]) for t in combinations]

    @property
    def dataset(self):
        """Get the nsITelemetry constant equivalent for release_channel_collection.
        """
        rcc = self._definition.get('release_channel_collection', 'opt-in')
        if rcc == 'opt-out':
            return 'nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT'
        else:
            return 'nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN'

    @property
    def extra_keys(self):
        return self._definition.get('extra_keys', {}).keys()


def load_events(filename):
    """Parses a YAML file containing the event definitions.

    :param filename: the YAML file containing the event definitions.
    :raises Exception: if the event file cannot be opened or parsed.
    """

    # Parse the event definitions from the YAML file.
    events = None
    try:
        with open(filename, 'r') as f:
            events = yaml.safe_load(f)
    except IOError, e:
        raise Exception('Error opening ' + filename + ': ' + e.message)
    except ValueError, e:
        raise Exception('Error parsing events in ' + filename + ': ' + e.message)

    event_list = []

    # Events are defined in a fixed two-level hierarchy within the definition file.
    # The first level contains the category (group name), while the second level contains
    # the event names and definitions, e.g.:
    #   category.name:
    #     event_name:
    #       <event definition>
    #      ...
    #   ...
    for category_name, category in events.iteritems():
        string_check("top level structure", field='category', value=category_name,
                     min_length=1, max_length=MAX_CATEGORY_NAME_LENGTH,
                     regex=IDENTIFIER_PATTERN)

        # Make sure that the category has at least one entry in it.
        if not category or len(category) == 0:
            raise ValueError(category_name + ' must contain at least one entry')

        for name, entry in category.iteritems():
            string_check(category_name, field='event name', value=name,
                         min_length=1, max_length=MAX_METHOD_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)
            event_list.append(EventData(category_name, name, entry))

    return event_list
