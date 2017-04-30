# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import yaml
import shared_telemetry_utils as utils

from shared_telemetry_utils import ParserError

# The map of containing the allowed scalar types and their mapping to
# nsITelemetry::SCALAR_* type constants.

BASE_DOC_URL = 'https://gecko.readthedocs.io/en/latest/toolkit/components/' + \
               'telemetry/telemetry/collection/scalars.html'

SCALAR_TYPES_MAP = {
    'uint': 'nsITelemetry::SCALAR_COUNT',
    'string': 'nsITelemetry::SCALAR_STRING',
    'boolean': 'nsITelemetry::SCALAR_BOOLEAN'
}


class ScalarType:
    """A class for representing a scalar definition."""

    def __init__(self, group_name, probe_name, definition):
        # Validate and set the name, so we don't need to pass it to the other
        # validation functions.
        self.validate_names(group_name, probe_name)
        self._name = probe_name
        self._group_name = group_name

        # Validating the scalar definition.
        self.validate_types(definition)
        self.validate_values(definition)

        # Everything is ok, set the rest of the data.
        self._definition = definition
        definition['expires'] = utils.add_expiration_postfix(definition['expires'])

    def validate_names(self, group_name, probe_name):
        """Validate the group and probe name:
            - Group name must be alpha-numeric + '.', no leading/trailing digit or '.'.
            - Probe name must be alpha-numeric + '_', no leading/trailing digit or '_'.

        :param group_name: the name of the group the probe is in.
        :param probe_name: the name of the scalar probe.
        :raises ParserError: if the length of the names exceeds the limit or they don't
                conform our name specification.
        """

        # Enforce a maximum length on group and probe names.
        MAX_NAME_LENGTH = 40
        for n in [group_name, probe_name]:
            if len(n) > MAX_NAME_LENGTH:
                raise ParserError(("Name '{}' exceeds maximum name length of {} characters.\n"
                                   "See: {}#the-yaml-definition-file")
                                  .format(n, MAX_NAME_LENGTH, BASE_DOC_URL))

        def check_name(name, error_msg_prefix, allowed_char_regexp):
            # Check if we only have the allowed characters.
            chars_regxp = r'^[a-zA-Z0-9' + allowed_char_regexp + r']+$'
            if not re.search(chars_regxp, name):
                raise ParserError((error_msg_prefix + " name must be alpha-numeric. Got: '{}'.\n"
                                  "See: {}#the-yaml-definition-file").format(name, BASE_DOC_URL))

            # Don't allow leading/trailing digits, '.' or '_'.
            if re.search(r'(^[\d\._])|([\d\._])$', name):
                raise ParserError((error_msg_prefix + " name must not have a leading/trailing "
                                   "digit, a dot or underscore. Got: '{}'.\n"
                                   " See: {}#the-yaml-definition-file").format(name, BASE_DOC_URL))

        check_name(group_name, 'Group', r'\.')
        check_name(probe_name, 'Probe', r'_')

    def validate_types(self, definition):
        """This function performs some basic sanity checks on the scalar definition:
            - Checks that all the required fields are available.
            - Checks that all the fields have the expected types.

        :param definition: the dictionary containing the scalar properties.
        :raises ParserError: if a scalar definition field is of the wrong type.
        :raises ParserError: if a required field is missing or unknown fields are present.
        """

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
        }

        # The types for the data within the fields that hold lists.
        LIST_FIELDS_CONTENT = {
            'bug_numbers': int,
            'notification_emails': basestring,
            'record_in_processes': basestring,
        }

        # Concatenate the required and optional field definitions.
        ALL_FIELDS = REQUIRED_FIELDS.copy()
        ALL_FIELDS.update(OPTIONAL_FIELDS)

        # Checks that all the required fields are available.
        missing_fields = [f for f in REQUIRED_FIELDS.keys() if f not in definition]
        if len(missing_fields) > 0:
            raise ParserError(self._name + ' - missing required fields: ' + ', '.join(missing_fields) +
                              '.\nSee: {}#required-fields'.format(BASE_DOC_URL))

        # Do we have any unknown field?
        unknown_fields = [f for f in definition.keys() if f not in ALL_FIELDS]
        if len(unknown_fields) > 0:
            raise ParserError(self._name + ' - unknown fields: ' + ', '.join(unknown_fields) +
                              '.\nSee: {}#required-fields'.format(BASE_DOC_URL))

        # Checks the type for all the fields.
        wrong_type_names = ['{} must be {}'.format(f, ALL_FIELDS[f].__name__)
                            for f in definition.keys() if not isinstance(definition[f], ALL_FIELDS[f])]
        if len(wrong_type_names) > 0:
            raise ParserError(self._name + ' - ' + ', '.join(wrong_type_names) +
                              '.\nSee: {}#required-fields'.format(BASE_DOC_URL))

        # Check that the lists are not empty and that data in the lists
        # have the correct types.
        list_fields = [f for f in definition if isinstance(definition[f], list)]
        for field in list_fields:
            # Check for empty lists.
            if len(definition[field]) == 0:
                raise ParserError(("Field '{}' for probe '{}' must not be empty" +
                                   ".\nSee: {}#required-fields)")
                                  .format(field, self._name, BASE_DOC_URL))
            # Check the type of the list content.
            broken_types =\
                [not isinstance(v, LIST_FIELDS_CONTENT[field]) for v in definition[field]]
            if any(broken_types):
                raise ParserError(("Field '{}' for probe '{}' must only contain values of type {}"
                                   ".\nSee: {}#the-yaml-definition-file)")
                                  .format(field, self._name, LIST_FIELDS_CONTENT[field].__name__,
                                          BASE_DOC_URL))

    def validate_values(self, definition):
        """This function checks that the fields have the correct values.

        :param definition: the dictionary containing the scalar properties.
        :raises ParserError: if a scalar definition field contains an unexpected value.
        """

        # Validate the scalar kind.
        scalar_kind = definition.get('kind')
        if scalar_kind not in SCALAR_TYPES_MAP.keys():
            raise ParserError(self._name + ' - unknown scalar kind: ' + scalar_kind +
                              '.\nSee: {}'.format(BASE_DOC_URL))

        # Validate the collection policy.
        collection_policy = definition.get('release_channel_collection', None)
        if collection_policy and collection_policy not in ['opt-in', 'opt-out']:
            raise ParserError(self._name + ' - unknown collection policy: ' + collection_policy +
                              '.\nSee: {}#optional-fields'.format(BASE_DOC_URL))

        # Validate the cpp_guard.
        cpp_guard = definition.get('cpp_guard')
        if cpp_guard and re.match(r'\W', cpp_guard):
            raise ParserError(self._name + ' - invalid cpp_guard: ' + cpp_guard +
                              '.\nSee: {}#optional-fields'.format(BASE_DOC_URL))

        # Validate record_in_processes.
        record_in_processes = definition.get('record_in_processes', [])
        for proc in record_in_processes:
            if not utils.is_valid_process_name(proc):
                raise ParserError(self._name + ' - unknown value in record_in_processes: ' + proc +
                                  '.\nSee: {}'.format(BASE_DOC_URL))

    @property
    def name(self):
        """Get the scalar name"""
        return self._name

    @property
    def label(self):
        """Get the scalar label generated from the scalar and group names."""
        return self._group_name + '.' + self._name

    @property
    def enum_label(self):
        """Get the enum label generated from the scalar and group names. This is used to
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
        return self._definition['expires']

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
        return self._definition['record_in_processes']

    @property
    def record_in_processes_enum(self):
        """Get the non-empty list of flags representing the processes to record data in"""
        return [utils.process_name_to_enum(p) for p in self.record_in_processes]

    @property
    def dataset(self):
        """Get the nsITelemetry constant equivalent to the chose release channel collection
        policy for the scalar.
        """
        # The collection policy is optional, but we still define a default
        # behaviour for it.
        rcc = self._definition.get('release_channel_collection', 'opt-in')
        table = {
            'opt-in': 'OPTIN',
            'opt-out': 'OPTOUT',
        }
        return 'nsITelemetry::DATASET_RELEASE_CHANNEL_' + table[rcc]

    @property
    def cpp_guard(self):
        """Get the cpp guard for this scalar"""
        return self._definition.get('cpp_guard')


def load_scalars(filename):
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
        raise ParserError('Error opening ' + filename + ': ' + e.message)
    except ValueError, e:
        raise ParserError('Error parsing scalars in {}: {}'
                          '.\nSee: {}'.format(filename, e.message, BASE_DOC_URL))

    scalar_list = []

    # Scalars are defined in a fixed two-level hierarchy within the definition file.
    # The first level contains the group name, while the second level contains the
    # probe name (e.g. "group.name: probe: ...").
    for group_name in scalars:
        group = scalars[group_name]

        # Make sure that the group has at least one probe in it.
        if not group or len(group) == 0:
            raise ParserError('Category "{}" must have at least one probe in it' +
                              '.\nSee: {}'.format(group_name, BASE_DOC_URL))

        for probe_name in group:
            # We found a scalar type. Go ahead and parse it.
            scalar_info = group[probe_name]
            scalar_list.append(ScalarType(group_name, probe_name, scalar_info))

    return scalar_list
