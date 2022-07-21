# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import re
import yaml
import atexit

from .shared_telemetry_utils import ParserError

atexit.register(ParserError.exit_func)

BASE_DOC_URL = (
    "https://firefox-source-docs.mozilla.org/toolkit/components/"
    + "telemetry/telemetry/collection/user_interactions.html"
)


class UserInteractionType:
    """A class for representing a UserInteraction definition."""

    def __init__(self, category_name, user_interaction_name, definition):
        # Validate and set the name, so we don't need to pass it to the other
        # validation functions.
        self.validate_names(category_name, user_interaction_name)
        self._name = user_interaction_name
        self._category_name = category_name

        # Validating the UserInteraction definition.
        self.validate_types(definition)

        # Everything is ok, set the rest of the data.
        self._definition = definition

    def validate_names(self, category_name, user_interaction_name):
        """Validate the category and UserInteraction name:
            - Category name must be alpha-numeric + '.', no leading/trailing digit or '.'.
            - UserInteraction name must be alpha-numeric + '_', no leading/trailing digit or '_'.

        :param category_name: the name of the category the UserInteraction is in.
        :param user_interaction_name: the name of the UserInteraction.
        :raises ParserError: if the length of the names exceeds the limit or they don't
                conform our name specification.
        """

        # Enforce a maximum length on category and UserInteraction names.
        MAX_NAME_LENGTH = 40
        for n in [category_name, user_interaction_name]:
            if len(n) > MAX_NAME_LENGTH:
                ParserError(
                    (
                        "Name '{}' exceeds maximum name length of {} characters.\n"
                        "See: {}#the-yaml-definition-file"
                    ).format(n, MAX_NAME_LENGTH, BASE_DOC_URL)
                ).handle_later()

        def check_name(name, error_msg_prefix, allowed_char_regexp):
            # Check if we only have the allowed characters.
            chars_regxp = r"^[a-zA-Z0-9" + allowed_char_regexp + r"]+$"
            if not re.search(chars_regxp, name):
                ParserError(
                    (
                        error_msg_prefix + " name must be alpha-numeric. Got: '{}'.\n"
                        "See: {}#the-yaml-definition-file"
                    ).format(name, BASE_DOC_URL)
                ).handle_later()

            # Don't allow leading/trailing digits, '.' or '_'.
            if re.search(r"(^[\d\._])|([\d\._])$", name):
                ParserError(
                    (
                        error_msg_prefix + " name must not have a leading/trailing "
                        "digit, a dot or underscore. Got: '{}'.\n"
                        " See: {}#the-yaml-definition-file"
                    ).format(name, BASE_DOC_URL)
                ).handle_later()

        check_name(category_name, "Category", r"\.")
        check_name(user_interaction_name, "UserInteraction", r"_")

    def validate_types(self, definition):
        """This function performs some basic sanity checks on the UserInteraction
           definition:
            - Checks that all the required fields are available.
            - Checks that all the fields have the expected types.

        :param definition: the dictionary containing the UserInteraction
               properties.
        :raises ParserError: if a UserInteraction definition field is of the
                wrong type.
        :raises ParserError: if a required field is missing or unknown fields are present.
        """

        # The required and optional fields in a UserInteraction definition.
        REQUIRED_FIELDS = {
            "bug_numbers": list,  # This contains ints. See LIST_FIELDS_CONTENT.
            "description": str,
        }

        # The types for the data within the fields that hold lists.
        LIST_FIELDS_CONTENT = {
            "bug_numbers": int,
        }

        ALL_FIELDS = REQUIRED_FIELDS.copy()

        # Checks that all the required fields are available.
        missing_fields = [f for f in REQUIRED_FIELDS.keys() if f not in definition]
        if len(missing_fields) > 0:
            ParserError(
                self._name
                + " - missing required fields: "
                + ", ".join(missing_fields)
                + ".\nSee: {}#required-fields".format(BASE_DOC_URL)
            ).handle_later()

        # Do we have any unknown field?
        unknown_fields = [f for f in definition.keys() if f not in ALL_FIELDS]
        if len(unknown_fields) > 0:
            ParserError(
                self._name
                + " - unknown fields: "
                + ", ".join(unknown_fields)
                + ".\nSee: {}#required-fields".format(BASE_DOC_URL)
            ).handle_later()

        # Checks the type for all the fields.
        wrong_type_names = [
            "{} must be {}".format(f, str(ALL_FIELDS[f]))
            for f in definition.keys()
            if not isinstance(definition[f], ALL_FIELDS[f])
        ]
        if len(wrong_type_names) > 0:
            ParserError(
                self._name
                + " - "
                + ", ".join(wrong_type_names)
                + ".\nSee: {}#required-fields".format(BASE_DOC_URL)
            ).handle_later()

        # Check that the lists are not empty and that data in the lists
        # have the correct types.
        list_fields = [f for f in definition if isinstance(definition[f], list)]
        for field in list_fields:
            # Check for empty lists.
            if len(definition[field]) == 0:
                ParserError(
                    (
                        "Field '{}' for probe '{}' must not be empty"
                        + ".\nSee: {}#required-fields)"
                    ).format(field, self._name, BASE_DOC_URL)
                ).handle_later()
            # Check the type of the list content.
            broken_types = [
                not isinstance(v, LIST_FIELDS_CONTENT[field]) for v in definition[field]
            ]
            if any(broken_types):
                ParserError(
                    (
                        "Field '{}' for probe '{}' must only contain values of type {}"
                        ".\nSee: {}#the-yaml-definition-file)"
                    ).format(
                        field,
                        self._name,
                        str(LIST_FIELDS_CONTENT[field]),
                        BASE_DOC_URL,
                    )
                ).handle_later()

    @property
    def category(self):
        """Get the category name"""
        return self._category_name

    @property
    def name(self):
        """Get the UserInteraction name"""
        return self._name

    @property
    def label(self):
        """Get the UserInteraction label generated from the UserInteraction
        and category names.
        """
        return self._category_name + "." + self._name

    @property
    def bug_numbers(self):
        """Get the list of related bug numbers"""
        return self._definition["bug_numbers"]

    @property
    def description(self):
        """Get the UserInteraction description"""
        return self._definition["description"]


def load_user_interactions(filename):
    """Parses a YAML file containing the UserInteraction definition.

    :param filename: the YAML file containing the UserInteraction definition.
    :raises ParserError: if the UserInteraction file cannot be opened or
            parsed.
    """

    # Parse the UserInteraction definitions from the YAML file.
    user_interactions = None
    try:
        with io.open(filename, "r", encoding="utf-8") as f:
            user_interactions = yaml.safe_load(f)
    except IOError as e:
        ParserError("Error opening " + filename + ": " + str(e)).handle_now()
    except ValueError as e:
        ParserError(
            "Error parsing UserInteractions in {}: {}"
            ".\nSee: {}".format(filename, e, BASE_DOC_URL)
        ).handle_now()

    user_interaction_list = []

    # UserInteractions are defined in a fixed two-level hierarchy within the
    # definition file. The first level contains the category name, while the
    # second level contains the UserInteraction name
    # (e.g. "category.name: user.interaction: ...").
    for category_name in sorted(user_interactions):
        category = user_interactions[category_name]

        # Make sure that the category has at least one UserInteraction in it.
        if not category or len(category) == 0:
            ParserError(
                'Category "{}" must have at least one UserInteraction in it'
                ".\nSee: {}".format(category_name, BASE_DOC_URL)
            ).handle_later()

        for user_interaction_name in sorted(category):
            # We found a UserInteraction type. Go ahead and parse it.
            user_interaction_info = category[user_interaction_name]
            user_interaction_list.append(
                UserInteractionType(
                    category_name, user_interaction_name, user_interaction_info
                )
            )

    return user_interaction_list


def from_files(filenames):
    all_user_interactions = []

    for filename in filenames:
        all_user_interactions += load_user_interactions(filename)

    for user_interaction in all_user_interactions:
        yield user_interaction
