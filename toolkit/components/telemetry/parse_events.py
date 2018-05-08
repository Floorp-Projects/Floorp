# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import yaml
import itertools
import string
import shared_telemetry_utils as utils

from shared_telemetry_utils import ParserError

MAX_CATEGORY_NAME_LENGTH = 30
MAX_METHOD_NAME_LENGTH = 20
MAX_OBJECT_NAME_LENGTH = 20
MAX_EXTRA_KEYS_COUNT = 10
MAX_EXTRA_KEY_NAME_LENGTH = 15

IDENTIFIER_PATTERN = r'^[a-zA-Z][a-zA-Z0-9_.]*[a-zA-Z0-9]$'


def nice_type_name(t):
    if issubclass(t, basestring):
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


class AtomicTypeChecker:
    """Validate a simple value against a given type"""
    def __init__(self, instance_type):
        self.instance_type = instance_type

    def check(self, identifier, key, value):
        if not isinstance(value, self.instance_type):
            ParserError("%s: Failed type check for %s - expected %s, got %s." %
                        (identifier, key, nice_type_name(self.instance_type),
                            nice_type_name(type(value)))).handle_later()


class MultiTypeChecker:
    """Validate a simple value against a list of possible types"""
    def __init__(self, *instance_types):
        if not instance_types:
            raise Exception("At least one instance type is required.")
        self.instance_types = instance_types

    def check(self, identifier, key, value):
        if not any(isinstance(value, i) for i in self.instance_types):
            ParserError("%s: Failed type check for %s - got %s, expected one of:\n%s" %
                        (identifier, key,
                         nice_type_name(type(value)),
                         " or ".join(map(nice_type_name, self.instance_types)))).handle_later()


class ListTypeChecker:
    """Validate a list of values against a given type"""
    def __init__(self, instance_type):
        self.instance_type = instance_type

    def check(self, identifier, key, value):
        if len(value) < 1:
            ParserError("%s: Failed check for %s - list should not be empty." %
                        (identifier, key)).handle_now()

        for x in value:
            if not isinstance(x, self.instance_type):
                ParserError("%s: Failed type check for %s - expected list value type %s, got"
                            " %s." % (identifier, key, nice_type_name(self.instance_type),
                                      nice_type_name(type(x)))).handle_later()


class DictTypeChecker:
    """Validate keys and values of a dict against a given type"""
    def __init__(self, keys_instance_type, values_instance_type):
        self.keys_instance_type = keys_instance_type
        self.values_instance_type = values_instance_type

    def check(self, identifier, key, value):
        if len(value.keys()) < 1:
            ParserError("%s: Failed check for %s - dict should not be empty." %
                        (identifier, key)).handle_now()
        for x in value.iterkeys():
            if not isinstance(x, self.keys_instance_type):
                ParserError("%s: Failed dict type check for %s - expected key type %s, got "
                            "%s." %
                            (identifier, key,
                             nice_type_name(self.keys_instance_type),
                             nice_type_name(type(x)))).handle_later()
        for k, v in value.iteritems():
            if not isinstance(v, self.values_instance_type):
                ParserError("%s: Failed dict type check for %s - "
                            "expected value type %s for key %s, got %s." %
                            (identifier, key,
                             nice_type_name(self.values_instance_type),
                             k, nice_type_name(type(v)))).handle_later()


def type_check_event_fields(identifier, name, definition):
    """Perform a type/schema check on the event definition."""
    REQUIRED_FIELDS = {
        'objects': ListTypeChecker(basestring),
        'bug_numbers': ListTypeChecker(int),
        'notification_emails': ListTypeChecker(basestring),
        'record_in_processes': ListTypeChecker(basestring),
        'description': AtomicTypeChecker(basestring),
    }
    OPTIONAL_FIELDS = {
        'methods': ListTypeChecker(basestring),
        'release_channel_collection': AtomicTypeChecker(basestring),
        'expiry_version': AtomicTypeChecker(basestring),
        'extra_keys': DictTypeChecker(basestring, basestring),
        'products': ListTypeChecker(basestring),
    }
    ALL_FIELDS = REQUIRED_FIELDS.copy()
    ALL_FIELDS.update(OPTIONAL_FIELDS)

    # Check that all the required fields are available.
    missing_fields = [f for f in REQUIRED_FIELDS.keys() if f not in definition]
    if len(missing_fields) > 0:
        ParserError(identifier + ': Missing required fields: ' + ', '.join(missing_fields)
                    ).handle_now()

    # Is there any unknown field?
    unknown_fields = [f for f in definition.keys() if f not in ALL_FIELDS]
    if len(unknown_fields) > 0:
        ParserError(identifier + ': Unknown fields: ' + ', '.join(unknown_fields)).handle_later()

    # Type-check fields.
    for k, v in definition.iteritems():
        ALL_FIELDS[k].check(identifier, k, v)


def string_check(identifier, field, value, min_length=1, max_length=None, regex=None):
    # Length check.
    if len(value) < min_length:
        ParserError("%s: Value '%s' for field %s is less than minimum length of %d." %
                    (identifier, value, field, min_length)).handle_later()
    if max_length and len(value) > max_length:
        ParserError("%s: Value '%s' for field %s is greater than maximum length of %d." %
                    (identifier, value, field, max_length)).handle_later()
    # Regex check.
    if regex and not re.match(regex, value):
        ParserError('%s: String value "%s" for %s is not matching pattern "%s".' %
                    (identifier, value, field, regex)).handle_later()


class EventData:
    """A class representing one event."""

    def __init__(self, category, name, definition, strict_type_checks=False):
        self._category = category
        self._name = name
        self._definition = definition
        self._strict_type_checks = strict_type_checks

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
        if rcc not in allowed_rcc:
            ParserError("%s: Value for %s should be one of: %s" %
                        (self.identifier, rcc_key, ", ".join(allowed_rcc))).handle_later()

        # Check record_in_processes.
        record_in_processes = definition.get('record_in_processes')
        for proc in record_in_processes:
            if not utils.is_valid_process_name(proc):
                ParserError(self.identifier + ': Unknown value in record_in_processes: ' +
                            proc).handle_later()

        # Check products.
        products = definition.get('products', [])
        for product in products:
            if not utils.is_valid_product(product):
                ParserError(self.identifier + ': Unknown value in products: ' +
                            product).handle_later()

        # Check extra_keys.
        extra_keys = definition.get('extra_keys', {})
        if len(extra_keys.keys()) > MAX_EXTRA_KEYS_COUNT:
            ParserError("%s: Number of extra_keys exceeds limit %d." %
                        (self.identifier, MAX_EXTRA_KEYS_COUNT)).handle_later()
        for key in extra_keys.iterkeys():
            string_check(self.identifier, field='extra_keys', value=key,
                         min_length=1, max_length=MAX_EXTRA_KEY_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)

        # Check expiry.
        if 'expiry_version' not in definition:
            ParserError("%s: event is missing required field expiry_version"
                        % (self.identifier)).handle_later()

        # Finish setup.
        # Historical versions of Events.yaml may contain expiration versions
        # using the deprecated format 'N.Na1'. Those scripts set
        # self._strict_type_checks to false.
        expiry_version = definition.get('expiry_version', 'never')
        if not utils.validate_expiration_version(expiry_version) and self._strict_type_checks:
            ParserError('{}: invalid expiry_version: {}.'
                        .format(self.identifier, expiry_version)).handle_now()
        definition['expiry_version'] = utils.add_expiration_postfix(expiry_version)

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
    def products(self):
        """Get the non-empty list of products to record data on"""
        return self._definition.get('products', ["firefox", "fennec"])

    @property
    def products_enum(self):
        """Get the non-empty list of flags representing products to record data on"""
        return [utils.product_name_to_enum(p) for p in self.products]

    @property
    def expiry_version(self):
        return self._definition.get('expiry_version')

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
        rcc = self.dataset_short
        if rcc == 'opt-out':
            return 'nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT'
        else:
            return 'nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN'

    @property
    def dataset_short(self):
        """Get the short name of the chosen release channel collection policy for the event.
        """
        # The collection policy is optional, but we still define a default
        # behaviour for it.
        return self._definition.get('release_channel_collection', 'opt-in')

    @property
    def extra_keys(self):
        return self._definition.get('extra_keys', {}).keys()


def load_events(filename, strict_type_checks):
    """Parses a YAML file containing the event definitions.

    :param filename: the YAML file containing the event definitions.
    :strict_type_checks A boolean indicating whether to use the stricter type checks.
    :raises ParserError: if the event file cannot be opened or parsed.
    """

    # Parse the event definitions from the YAML file.
    events = None
    try:
        with open(filename, 'r') as f:
            events = yaml.safe_load(f)
    except IOError, e:
        ParserError('Error opening ' + filename + ': ' + e.message + ".").handle_now()
    except ParserError, e:
        ParserError('Error parsing events in ' + filename + ': ' + e.message + ".").handle_now()

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
            ParserError('Category ' + category_name + ' must contain at least one entry.'
                        ).handle_now()

        for name, entry in category.iteritems():
            string_check(category_name, field='event name', value=name,
                         min_length=1, max_length=MAX_METHOD_NAME_LENGTH,
                         regex=IDENTIFIER_PATTERN)
            event_list.append(EventData(category_name, name, entry, strict_type_checks))

    return event_list
