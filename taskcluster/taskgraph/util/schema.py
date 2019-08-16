# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re
import pprint
import collections
import voluptuous

from six import text_type

import taskgraph

from mozbuild import schedules

from .keyed_by import evaluate_keyed_by


def validate_schema(schema, obj, msg_prefix):
    """
    Validate that object satisfies schema.  If not, generate a useful exception
    beginning with msg_prefix.
    """
    if taskgraph.fast:
        return
    try:
        schema(obj)
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

    a call to `resolve_keyed_by(item, 'job.chunks', item['thing-name'])`
    would mutate item in-place to::

        job:
            test-platform: linux128
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

    container[subfield] = evaluate_keyed_by(
        value=container[subfield],
        item_name="`{}` in `{}`".format(field, item_name),
        attributes=dict(item, **extra_values),
    )

    return item


# Schemas for YAML files should use dashed identifiers by default.  If there are
# components of the schema for which there is a good reason to use another format,
# they can be whitelisted here.
WHITELISTED_SCHEMA_IDENTIFIERS = [
    # upstream-artifacts are handed directly to scriptWorker, which expects interCaps
    lambda path: "[u'upstream-artifacts']" in path,
]


def check_schema(schema):
    identifier_re = re.compile('^[a-z][a-z0-9-]*$')

    def whitelisted(path):
        return any(f(path) for f in WHITELISTED_SCHEMA_IDENTIFIERS)

    def iter(path, sch):
        def check_identifier(path, k):
            if k in (basestring, text_type, voluptuous.Extra):
                pass
            elif isinstance(k, basestring):
                if not identifier_re.match(k) and not whitelisted(path):
                    raise RuntimeError(
                        'YAML schemas should use dashed lower-case identifiers, '
                        'not {!r} @ {}'.format(k, path))
            elif isinstance(k, (voluptuous.Optional, voluptuous.Required)):
                check_identifier(path, k.schema)
            elif isinstance(k, voluptuous.Any):
                for v in k.validators:
                    check_identifier(path, v)
            elif not whitelisted(path):
                raise RuntimeError(
                    'Unexpected type in YAML schema: {} @ {}'.format(
                        type(k).__name__, path))

        if isinstance(sch, collections.Mapping):
            for k, v in sch.iteritems():
                child = "{}[{!r}]".format(path, k)
                check_identifier(child, k)
                iter(child, v)
        elif isinstance(sch, (list, tuple)):
            for i, v in enumerate(sch):
                iter("{}[{}]".format(path, i), v)
        elif isinstance(sch, voluptuous.Any):
            for v in sch.validators:
                iter(path, v)
    iter('schema', schema.schema)


class Schema(voluptuous.Schema):
    """
    Operates identically to voluptuous.Schema, but applying some taskgraph-specific checks
    in the process.
    """
    def __init__(self, *args, **kwargs):
        super(Schema, self).__init__(*args, **kwargs)
        check_schema(self)

    def extend(self, *args, **kwargs):
        schema = super(Schema, self).extend(*args, **kwargs)
        check_schema(schema)
        # We want twice extend schema to be checked too.
        schema.__class__ = Schema
        return schema

    def __getitem__(self, item):
        return self.schema[item]


OptimizationSchema = voluptuous.Any(
    # always run this task (default)
    None,
    # search the index for the given index namespaces, and replace this task if found
    # the search occurs in order, with the first match winning
    {'index-search': [basestring]},
    # consult SETA and skip this task if it is low-value
    {'seta': None},
    # skip this task if none of the given file patterns match
    {'skip-unless-changed': [basestring]},
    # skip this task if unless the change files' SCHEDULES contains any of these components
    {'skip-unless-schedules': list(schedules.ALL_COMPONENTS)},
    # skip if SETA or skip-unless-schedules says to
    {'skip-unless-schedules-or-seta': list(schedules.ALL_COMPONENTS)},
)

# shortcut for a string where task references are allowed
taskref_or_string = voluptuous.Any(
    basestring,
    {voluptuous.Required('task-reference'): basestring},
    {voluptuous.Required('artifact-reference'): basestring},
)
