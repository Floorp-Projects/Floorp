# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re
import copy
import pprint
import voluptuous


def validate_schema(schema, obj, msg_prefix):
    """
    Validate that object satisfies schema.  If not, generate a useful exception
    beginning with msg_prefix.
    """
    try:
        # deep copy the result since it may include mutable defaults
        return copy.deepcopy(schema(obj))
    except voluptuous.MultipleInvalid as exc:
        msg = [msg_prefix]
        for error in exc.errors:
            msg.append(str(error))
        raise Exception('\n'.join(msg) + '\n' + pprint.pformat(obj))


def optionally_keyed_by(*arguments):
    """
    Mark a schema value as optionally keyed by any of a number of fields.  The
    schema is the last argument, and the remaining fields are taken to be the
    field names.  For example:

        'some-value': optionally_keyed_by(
            'test-platform', 'build-platform',
            Any('a', 'b', 'c'))
    """
    subschema = arguments[-1]
    fields = arguments[:-1]
    options = [subschema]
    for field in fields:
        options.append({'by-' + field: {basestring: subschema}})
    return voluptuous.Any(*options)


def resolve_keyed_by(item, field, item_name, **extra_values):
    """
    For values which can either accept a literal value, or be keyed by some
    other attribute of the item, perform that lookup and replacement in-place
    (modifying `item` directly).  The field is specified using dotted notation
    to traverse dictionaries.

    For example, given item

        job:
            test-platform: linux128
            chunks:
                by-test-platform:
                    macosx-10.11/debug: 13
                    win.*: 6
                    default: 12

    a call to `resolve_keyed_by(item, 'job.chunks', item['thing-name'])
    would mutate item in-place to

        job:
            chunks: 12

    The `item_name` parameter is used to generate useful error messages.

    If extra_values are supplied, they represent additional values available
    for reference from by-<field>.
    """
    # find the field, returning the item unchanged if anything goes wrong
    container, subfield = item, field
    while '.' in subfield:
        f, subfield = subfield.split('.', 1)
        if f not in container:
            return item
        container = container[f]
        if not isinstance(container, dict):
            return item

    if subfield not in container:
        return item
    value = container[subfield]
    if not isinstance(value, dict) or len(value) != 1 or not value.keys()[0].startswith('by-'):
        return item

    keyed_by = value.keys()[0][3:]  # strip off 'by-' prefix
    key = extra_values.get(keyed_by) if keyed_by in extra_values else item[keyed_by]
    alternatives = value.values()[0]

    # exact match
    if key in alternatives:
        container[subfield] = alternatives[key]
        return item

    # regular expression match
    matches = [(k, v) for k, v in alternatives.iteritems() if re.match(k + '$', key)]
    if len(matches) > 1:
        raise Exception(
            "Multiple matching values for {} {!r} found while determining item {} in {}".format(
                keyed_by, key, field, item_name))
    elif matches:
        container[subfield] = matches[0][1]
        return item

    # default
    if 'default' in alternatives:
        container[subfield] = alternatives['default']
        return item

    raise Exception(
        "No {} matching {!r} nor 'default' found while determining item {} in {}".format(
            keyed_by, key, field, item_name))
