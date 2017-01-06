# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re
import copy
import pprint
import voluptuous


class TransformConfig(object):
    """A container for configuration affecting transforms.  The `config`
    argument to transforms is an instance of this class, possibly with
    additional kind-specific attributes beyond those set here."""
    def __init__(self, kind, path, config, params):
        # the name of the current kind
        self.kind = kind

        # the path to the kind configuration directory
        self.path = path

        # the parsed contents of kind.yml
        self.config = config

        # the parameters for this task-graph generation run
        self.params = params


class TransformSequence(object):
    """
    Container for a sequence of transforms.  Each transform is represented as a
    callable taking (config, items) and returning a generator which will yield
    transformed items.  The resulting sequence has the same interface.

    This is convenient to use in a file full of transforms, as it provides a
    decorator, @transforms.add, that will add the decorated function to the
    sequence.
    """

    def __init__(self, transforms=None):
        self.transforms = transforms or []

    def __call__(self, config, items):
        for xform in self.transforms:
            items = xform(config, items)
            if items is None:
                raise Exception("Transform {} is not a generator".format(xform))
        return items

    def __repr__(self):
        return '\n'.join(
            ['TransformSequence(['] +
            [repr(x) for x in self.transforms] +
            ['])'])

    def add(self, func):
        self.transforms.append(func)
        return func


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


def resolve_keyed_by(item, field, item_name):
    """
    For values which can either accept a literal value, or be keyed by some
    other attribute of the item, perform that lookup and replacement in-place
    (modifying `item` directly).  The field is specified using dotted notation
    to traverse dictionaries.

    For example, given item

        job:
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
    key = item[keyed_by]
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
