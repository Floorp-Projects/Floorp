# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import yaml
import atexit
import shared_telemetry_utils as utils

from shared_telemetry_utils import ParserError
atexit.register(ParserError.exit_func)

# The map of containing the allowed scalar types and their mapping to
# nsITelemetry::SCALAR_TYPE_* type constants.

BASE_DOC_URL = 'https://firefox-source-docs.mozilla.org/toolkit/components/' + \
               'telemetry/telemetry/collection/scalars.html'

SCALAR_TYPES_MAP = {
    'uint': 'nsITelemetry::SCALAR_TYPE_COUNT',
    'string': 'nsITelemetry::SCALAR_TYPE_STRING',
    'boolean': 'nsITelemetry::SCALAR_TYPE_BOOLEAN'
}


class ScalarType:
    """A class for representing a scalar definition."""

    def __init__(self, category_name, probe_name, definition, strict_type_checks):
        # Validate and set the name, so we don't need to pass it to the other
        # validation functions.
        self._strict_type_checks = strict_type_checks
        self.validate_names(category_name, probe_name)
        self._name = probe_name
        self._category_name = category_name

        # Validating the scalar definition.
        self.validate_types(definition)
        self.validate_values(definition)

        # Everything is ok, set the rest of the data.
        self._definition = definition
        self._expires = utils.add_expiration_postfix(definition['expires'])

    def validate_names(self, category_name, probe_name):
        """Validate the category and probe name:
            - Category name must be alpha-numeric + '.', no leading/trailing digit or '.'.
            - Probe name must be alpha-numeric + '_', no leading/trailing digit or '_'.

        :param category_name: the name of the category the probe is in.
        :param probe_name: the name of the scalar probe.
        :raises ParserError: if the length of the names exceeds the limit or they don't
                conform our name specification.
        """

        # Enforce a maximum length on category and probe names.
        MAX_NAME_LENGTH = 40
        for n in [category_name, probe_name]:
            if len(n) > MAX_NAME_LENGTH:
                ParserError(("Name '{}' exceeds maximum name length of {} characters.\n"
                             "See: {}#the-yaml-definition-file")
                            .format(n, MAX_NAME_LENGTH, BASE_DOC_URL)).handle_later()

        def check_name(name, error_msg_prefix, allowed_char_regexp):
            # Check if we only have the allowed characters.
            chars_regxp = r'^[a-zA-Z0-9' + allowed_char_regexp + r']+$'
            if not re.search(chars_regxp, name):
                ParserError((error_msg_prefix + " name must be alpha-numeric. Got: '{}'.\n"
                             "See: {}#the-yaml-definition-file")
                            .format(name, BASE_DOC_URL)).handle_later()

            # Don't allow leading/trailing digits, '.' or '_'.
            if re.search(r'(^[\d\._])|([\d\._])$', name):
                ParserError((error_msg_prefix + " name must not have a leading/trailing "
                             "digit, a dot or underscore. Got: '{}'.\n"
                             " See: {}#the-yaml-definition-file")
                            .format(name, BASE_DOC_URL)).handle_later()

        check_name(category_name, 'Category', r'\.')
        check_name(probe_name, 'Probe', r'_')

    def validate_types(self, definition):
        """This function performs some basic sanity checks on the scalar definition:
            - Checks that all the required fields are available.
            - Checks that all the fields have the expected types.

        :param definition: the dictionary containing the scalar properties.
        :raises ParserError: if a scalar definition field is of the wrong type.
        :raises ParserError: if a required field is missing or unknown fields are present.
        """

        if not self._strict_type_checks:
            return

        def validate_notification_email(notification_email):
            # Perform simple email validation to make sure it doesn't contain spaces or commas.
            return not any(c in notification_email for c in [',', ' '])

        # The required and optional fields in a scalar type definition.
        REQUIRED_FIELDS = {
            'bug_numbers': list,  # This contains ints. See LIST_FIELDS_CONTENT.
            'description': basestring,
            'expires': basestring,
            'kind': basestring,
            'notification_emails': list,  # This contains strings. See LIST_FIELDS_CONTENT.
            'record_in_processes': list,
        }

        OPTIONAL_FIELDS = {
            'cpp_guard': basestring,
            'release_channel_collection': basestring,
            'keyed': bool,
            'products': list,
        }

        # The types for the data within the fields that hold lists.
        LIST_FIELDS_CONTENT = {
            'bug_numbers': int,
            'notification_emails': basestring,
            'record_in_processes': basestring,
            'products': basestring,
        }

        # Concatenate the required and optional field definitions.
        ALL_FIELDS = REQUIRED_FIELDS.copy()
        ALL_FIELDS.update(OPTIONAL_FIELDS)

        # Checks that all the required fields are available.
        missing_fields = [f for f in REQUIRED_FIELDS.keys() if f not in definition]
        if len(missing_fields) > 0:
            ParserError(self._name + ' - missing required fields: ' +
                        ', '.join(missing_fields) +
                        '.\nSee: {}#required-fields'.format(BASE_DOC_URL)).handle_later()

        # Do we have any unknown field?
        unknown_fields = [f for f in definition.keys() if f not in ALL_FIELDS]
        if len(unknown_fields) > 0:
            ParserError(self._name + ' - unknown fields: ' + ', '.join(unknown_fields) +
                        '.\nSee: {}#required-fields'.format(BASE_DOC_URL)).handle_later()

        # Checks the type for all the fields.
        wrong_type_names = ['{} must be {}'.format(f, ALL_FIELDS[f].__name__)
                            for f in definition.keys()
                            if not isinstance(definition[f], ALL_FIELDS[f])]
        if len(wrong_type_names) > 0:
            ParserError(self._name + ' - ' + ', '.join(wrong_type_names) +
                        '.\nSee: {}#required-fields'.format(BASE_DOC_URL)).handle_later()

        # Check that the email addresses doesn't contain spaces or commas
        notification_emails = definition.get('notification_emails')
        for notification_email in notification_emails:
            if not validate_notification_email(notification_email):
                ParserError(self._name + ' - invalid email address: ' + notification_email +
                            '.\nSee: {}'.format(BASE_DOC_URL)).handle_later()

        # Check that the lists are not empty and that data in the lists
        # have the correct types.
        list_fields = [f for f in definition if isinstance(definition[f], list)]
        for field in list_fields:
            # Check for empty lists.
            if len(definition[field]) == 0:
                ParserError(("Field '{}' for probe '{}' must not be empty" +
                             ".\nSee: {}#required-fields)")
                            .format(field, self._name, BASE_DOC_URL)).handle_later()
            # Check the type of the list content.
            broken_types =\
                [not isinstance(v, LIST_FIELDS_CONTENT[field]) for v in definition[field]]
            if any(broken_types):
                ParserError(("Field '{}' for probe '{}' must only contain values of type {}"
                             ".\nSee: {}#the-yaml-definition-file)")
                            .format(field, self._name, LIST_FIELDS_CONTENT[field].__name__,
                                    BASE_DOC_URL)).handle_later()

    def validate_values(self, definition):
        """This function checks that the fields have the correct values.

        :param definition: the dictionary containing the scalar properties.
        :raises ParserError: if a scalar definition field contains an unexpected value.
        """

        if not self._strict_type_checks:
            return

        # Validate the scalar kind.
        scalar_kind = definition.get('kind')
        if scalar_kind not in SCALAR_TYPES_MAP.keys():
            ParserError(self._name + ' - unknown scalar kind: ' + scalar_kind +
                        '.\nSee: {}'.format(BASE_DOC_URL)).handle_later()

        # Validate the collection policy.
        collection_policy = definition.get('release_channel_collection', None)
        if collection_policy and collection_policy not in ['opt-in', 'opt-out']:
            ParserError(self._name + ' - unknown collection policy: ' + collection_policy +
                        '.\nSee: {}#optional-fields'.format(BASE_DOC_URL)).handle_later()

        # Validate the cpp_guard.
        cpp_guard = definition.get('cpp_guard')
        if cpp_guard and re.match(r'\W', cpp_guard):
            ParserError(self._name + ' - invalid cpp_guard: ' + cpp_guard +
                        '.\nSee: {}#optional-fields'.format(BASE_DOC_URL)).handle_later()

        # Validate record_in_processes.
        record_in_processes = definition.get('record_in_processes', [])
        for proc in record_in_processes:
            if not utils.is_valid_process_name(proc):
                ParserError(self._name + ' - unknown value in record_in_processes: ' + proc +
                            '.\nSee: {}'.format(BASE_DOC_URL)).handle_later()

        # Validate product.
        products = definition.get('products', [])
        for product in products:
            if not utils.is_valid_product(product):
                ParserError(self._name + ' - unknown value in products: ' + product +
                            '.\nSee: {}'.format(BASE_DOC_URL)).handle_later()

        # Validate the expiration version.
        # Historical versions of Scalars.json may contain expiration versions
        # using the deprecated format 'N.Na1'. Those scripts set
        # self._strict_type_checks to false.
        expires = definition.get('expires')
        if not utils.validate_expiration_version(expires) and self._strict_type_checks:
            ParserError('{} - invalid expires: {}.\nSee: {}#required-fields'
                        .format(self._name, expires, BASE_DOC_URL)).handle_later()

    @property
    def category(self):
        """Get the category name"""
        return self._category_name

    @property
    def name(self):
        """Get the scalar name"""
        return self._name

    @property
    def label(self):
        """Get the scalar label generated from the scalar and category names."""
        return self._category_name + '.' + self._name

    @property
    def enum_label(self):
        """Get the enum label generated from the scalar and category names. This is used to
        generate the enum tables."""

        # The scalar name can contain informations about its hierarchy (e.g. 'a.b.scalar').
        # We can't have dots in C++ enums, replace them with an underscore. Also, make the
        # label upper case for consistency with the histogram enums.
        return self.label.replace('.', '_').upper()

    @property
    def bug_numbers(self):
        """Get the list of related bug numbers"""
        return self._definition['bug_numbers']

    @property
    def description(self):
        """Get the scalar description"""
        return self._definition['description']

    @property
    def expires(self):
        """Get the scalar expiration"""
        return self._expires

    @property
    def kind(self):
        """Get the scalar kind"""
        return self._definition['kind']

    @property
    def keyed(self):
        """Boolean indicating whether this is a keyed scalar"""
        return self._definition.get('keyed', False)

    @property
    def nsITelemetry_kind(self):
        """Get the scalar kind constant defined in nsITelemetry"""
        return SCALAR_TYPES_MAP.get(self.kind)

    @property
    def notification_emails(self):
        """Get the list of notification emails"""
        return self._definition['notification_emails']

    @property
    def record_in_processes(self):
        """Get the non-empty list of processes to record data in"""
        # Before we added content process support in bug 1278556, we only recorded in the
        # main process.
        return self._definition.get('record_in_processes', ["main"])

    @property
    def record_in_processes_enum(self):
        """Get the non-empty list of flags representing the processes to record data in"""
        return [utils.process_name_to_enum(p) for p in self.record_in_processes]

    @property
    def products(self):
        """Get the non-empty list of products to record data on"""
        return self._definition.get('products', ["all"])

    @property
    def products_enum(self):
        """Get the non-empty list of flags representing products to record data on"""
        return [utils.product_name_to_enum(p) for p in self.products]

    @property
    def dataset(self):
        """Get the nsITelemetry constant equivalent to the chosen release channel collection
        policy for the scalar.
        """
        rcc = self.dataset_short
        table = {
            'opt-in': 'OPTIN',
            'opt-out': 'OPTOUT',
        }
        return 'nsITelemetry::DATASET_RELEASE_CHANNEL_' + table[rcc]

    @property
    def dataset_short(self):
        """Get the short name of the chosen release channel collection policy for the scalar.
        """
        # The collection policy is optional, but we still define a default
        # behaviour for it.
        return self._definition.get('release_channel_collection', 'opt-in')

    @property
    def cpp_guard(self):
        """Get the cpp guard for this scalar"""
        return self._definition.get('cpp_guard')


def load_scalars(filename, strict_type_checks=True):
    """Parses a YAML file containing the scalar definition.

    :param filename: the YAML file containing the scalars definition.
    :raises ParserError: if the scalar file cannot be opened or parsed.
    """

    # Parse the scalar definitions from the YAML file.
    scalars = None
    try:
        with open(filename, 'r') as f:
            scalars = yaml.safe_load(f)
    except IOError, e:
        ParserError('Error opening ' + filename + ': ' + e.message).handle_now()
    except ValueError, e:
        ParserError('Error parsing scalars in {}: {}'
                    '.\nSee: {}'.format(filename, e.message, BASE_DOC_URL)).handle_now()

    scalar_list = []

    # Scalars are defined in a fixed two-level hierarchy within the definition file.
    # The first level contains the category name, while the second level contains the
    # probe name (e.g. "category.name: probe: ...").
    for category_name in scalars:
        category = scalars[category_name]

        # Make sure that the category has at least one probe in it.
        if not category or len(category) == 0:
            ParserError('Category "{}" must have at least one probe in it'
                        '.\nSee: {}'.format(category_name, BASE_DOC_URL)).handle_later()

        for probe_name in category:
            # We found a scalar type. Go ahead and parse it.
            scalar_info = category[probe_name]
            scalar_list.append(
                ScalarType(category_name, probe_name, scalar_info, strict_type_checks))

    return scalar_list
