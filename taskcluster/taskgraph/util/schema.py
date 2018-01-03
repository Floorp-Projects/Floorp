# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re
import copy
import pprint
import collections
import voluptuous

from .attributes import keymatch


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

    The resulting schema will allow nesting of `by-test-platform` and
    `by-build-platform` in either order.
    """
    schema = arguments[-1]
    fields = arguments[:-1]

    # build the nestable schema by generating schema = Any(schema,
    # by-fld1, by-fld2, by-fld3) once for each field.  So we don't allow
    # infinite nesting, but one level of nesting for each field.
    for _ in arguments:
        options = [schema]
        for field in fields:
            options.append({'by-' + field: {basestring: schema}})
        schema = voluptuous.Any(*options)
    return schema


def resolve_keyed_by(item, field, item_name, **extra_values):
    """
    For values which can either accept a literal value, or be keyed by some
    other attribute of the item, perform that lookup and replacement in-place
    (modifying `item` directly).  The field is specified using dotted notation
    to traverse dictionaries.

    For example, given item::

        job:
            test-platform: linux128
            chunks:
                by-test-platform:
                    macosx-10.11/debug: 13
                    win.*: 6
                    default: 12

    a call to `resolve_keyed_by(item, 'job.chunks', item['thing-name'])
    would mutate item in-place to::

        job:
            chunks: 12

    The `item_name` parameter is used to generate useful error messages.

    If extra_values are supplied, they represent additional values available
    for reference from by-<field>.

    Items can be nested as deeply as the schema will allow::

        chunks:
            by-test-platform:
                win.*:
                    by-project:
                        ash: ..
                        cedar: ..
                linux: 13
                default: 12
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
    while True:
        if not isinstance(value, dict) or len(value) != 1 or not value.keys()[0].startswith('by-'):
            return item

        keyed_by = value.keys()[0][3:]  # strip off 'by-' prefix
        key = extra_values.get(keyed_by) if keyed_by in extra_values else item[keyed_by]
        alternatives = value.values()[0]

        if len(alternatives) == 1 and 'default' in alternatives:
            # Error out when only 'default' is specified as only alternatives,
            # because we don't need to by-{keyed_by} there.
            raise Exception(
                "Keyed-by '{}' unnecessary with only value 'default' "
                "found, when determining item '{}' in '{}'".format(
                    keyed_by, field, item_name))

        matches = keymatch(alternatives, key)
        if len(matches) > 1:
            raise Exception(
                "Multiple matching values for {} {!r} found while "
                "determining item {} in {}".format(
                    keyed_by, key, field, item_name))
        elif matches:
            value = container[subfield] = matches[0]
            continue

        raise Exception(
            "No {} matching {!r} nor 'default' found while determining item {} in {}".format(
                keyed_by, key, field, item_name))


# Schemas for YAML files should use dashed identifiers by default.  If there are
# components of the schema for which there is a good reason to use another format,
# they can be whitelisted here.
WHITELISTED_SCHEMA_IDENTIFIERS = [
    # upstream-artifacts are handed directly to scriptWorker, which expects interCaps
    lambda path: "[u'upstream-artifacts']" in path,
    # bbb release promotion properties
    lambda path: path.endswith("[u'build_number']"),
    lambda path: path.endswith("[u'tuxedo_server_url']"),
    lambda path: path.endswith("[u'release_promotion']"),
    lambda path: path.endswith("[u'generate_bz2_blob']"),
]


def check_schema(schema):
    identifier_re = re.compile('^[a-z][a-z0-9-]*$')

    def whitelisted(path):
        return any(f(path) for f in WHITELISTED_SCHEMA_IDENTIFIERS)

    def iter(path, sch):
        if isinstance(sch, collections.Mapping):
            for k, v in sch.iteritems():
                child = "{}[{!r}]".format(path, k)
                if isinstance(k, (voluptuous.Optional, voluptuous.Required)):
                    k = str(k)
                if isinstance(k, basestring):
                    if not identifier_re.match(k) and not whitelisted(child):
                        raise RuntimeError(
                            'YAML schemas should use dashed lower-case identifiers, '
                            'not {!r} @ {}'.format(k, child))
                iter(child, v)
        elif isinstance(sch, (list, tuple)):
            for i, v in enumerate(sch):
                iter("{}[{}]".format(path, i), v)
        elif isinstance(sch, voluptuous.Any):
            for v in sch.validators:
                iter(path, v)
    iter('schema', schema.schema)


def Schema(*args, **kwargs):
    """
    Operates identically to voluptuous.Schema, but applying some taskgraph-specific checks
    in the process.
    """
    schema = voluptuous.Schema(*args, **kwargs)
    check_schema(schema)
    return schema
