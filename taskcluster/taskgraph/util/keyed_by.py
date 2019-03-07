# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .attributes import keymatch


def evaluate_keyed_by(value, item_name, attributes):
    """
    For values which can either accept a literal value, or be keyed by some
    attributes, perform that lookup and return the result.

    For example, given item::

        by-test-platform:
            macosx-10.11/debug: 13
            win.*: 6
            default: 12

    a call to `evaluate_keyed_by(item, 'thing-name', {'test-platform': 'linux96')`
    would return `12`.

    The `item_name` parameter is used to generate useful error messages.
    Items can be nested as deeply as desired::

        by-test-platform:
            win.*:
                by-project:
                    ash: ..
                    cedar: ..
            linux: 13
            default: 12
    """
    while True:
        if not isinstance(value, dict) or len(value) != 1 or not value.keys()[0].startswith('by-'):
            return value

        keyed_by = value.keys()[0][3:]  # strip off 'by-' prefix
        key = attributes.get(keyed_by)
        alternatives = value.values()[0]

        if len(alternatives) == 1 and 'default' in alternatives:
            # Error out when only 'default' is specified as only alternatives,
            # because we don't need to by-{keyed_by} there.
            raise Exception(
                "Keyed-by '{}' unnecessary with only value 'default' "
                "found, when determining item {}".format(
                    keyed_by, item_name))

        if key is None:
            if 'default' in alternatives:
                value = alternatives['default']
                continue
            else:
                raise Exception(
                    "No attribute {} and no value for 'default' found "
                    "while determining item {}".format(
                        keyed_by, item_name))

        matches = keymatch(alternatives, key)
        if len(matches) > 1:
            raise Exception(
                "Multiple matching values for {} {!r} found while "
                "determining item {}".format(
                    keyed_by, key, item_name))
        elif matches:
            value = matches[0]
            continue

        raise Exception(
            "No {} matching {!r} nor 'default' found while determining item {}".format(
                keyed_by, key, item_name))
