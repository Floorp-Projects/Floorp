# encoding: utf-8
#
# Copyright (C) 2010-2013 Alec Thomas <alec@swapoff.org>
# All rights reserved.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution.
#
# Author: Alec Thomas <alec@swapoff.org>

"""Schema validation for Python data structures.

Given eg. a nested data structure like this:

    {
        'exclude': ['Users', 'Uptime'],
        'include': [],
        'set': {
            'snmp_community': 'public',
            'snmp_timeout': 15,
            'snmp_version': '2c',
        },
        'targets': {
            'localhost': {
                'exclude': ['Uptime'],
                'features': {
                    'Uptime': {
                        'retries': 3,
                    },
                    'Users': {
                        'snmp_community': 'monkey',
                        'snmp_port': 15,
                    },
                },
                'include': ['Users'],
                'set': {
                    'snmp_community': 'monkeys',
                },
            },
        },
    }

A schema like this:

    >>> settings = {
    ...   'snmp_community': str,
    ...   'retries': int,
    ...   'snmp_version': All(Coerce(str), Any('3', '2c', '1')),
    ... }
    >>> features = ['Ping', 'Uptime', 'Http']
    >>> schema = Schema({
    ...    'exclude': features,
    ...    'include': features,
    ...    'set': settings,
    ...    'targets': {
    ...      'exclude': features,
    ...      'include': features,
    ...      'features': {
    ...        str: settings,
    ...      },
    ...    },
    ... })

Validate like so:

    >>> schema({
    ...   'set': {
    ...     'snmp_community': 'public',
    ...     'snmp_version': '2c',
    ...   },
    ...   'targets': {
    ...     'exclude': ['Ping'],
    ...     'features': {
    ...       'Uptime': {'retries': 3},
    ...       'Users': {'snmp_community': 'monkey'},
    ...     },
    ...   },
    ... }) == {
    ...   'set': {'snmp_version': '2c', 'snmp_community': 'public'},
    ...   'targets': {
    ...     'exclude': ['Ping'],
    ...     'features': {'Uptime': {'retries': 3},
    ...                  'Users': {'snmp_community': 'monkey'}}}}
    True
"""
import collections
import datetime
import inspect
import os
import re
import sys
from contextlib import contextmanager
from functools import wraps


if sys.version_info >= (3,):
    import urllib.parse as urlparse
    long = int
    unicode = str
    basestring = str
    ifilter = filter
    iteritems = lambda d: d.items()
else:
    from itertools import ifilter
    import urlparse
    iteritems = lambda d: d.iteritems()


__author__ = 'Alec Thomas <alec@swapoff.org>'
__version__ = '0.8.11'


@contextmanager
def raises(exc, msg=None, regex=None):
    try:
        yield
    except exc as e:
        if msg is not None:
            assert str(e) == msg, '%r != %r' % (str(e), msg)
        if regex is not None:
            assert re.search(regex, str(e)), '%r does not match %r' % (str(e), regex)


class Undefined(object):
    def __nonzero__(self):
        return False

    def __repr__(self):
        return '...'


UNDEFINED = Undefined()


def default_factory(value):
    if value is UNDEFINED or callable(value):
        return value
    return lambda: value


# options for extra keys
PREVENT_EXTRA = 0  # any extra key not in schema will raise an error
ALLOW_EXTRA = 1    # extra keys not in schema will be included in output
REMOVE_EXTRA = 2   # extra keys not in schema will be excluded from output


class Error(Exception):
    """Base validation exception."""


class SchemaError(Error):
    """An error was encountered in the schema."""


class Invalid(Error):
    """The data was invalid.

    :attr msg: The error message.
    :attr path: The path to the error, as a list of keys in the source data.
    :attr error_message: The actual error message that was raised, as a
        string.

    """

    def __init__(self, message, path=None, error_message=None, error_type=None):
        Error.__init__(self, message)
        self.path = path or []
        self.error_message = error_message or message
        self.error_type = error_type

    @property
    def msg(self):
        return self.args[0]

    def __str__(self):
        path = ' @ data[%s]' % ']['.join(map(repr, self.path)) \
            if self.path else ''
        output = Exception.__str__(self)
        if self.error_type:
            output += ' for ' + self.error_type
        return output + path

    def prepend(self, path):
        self.path = path + self.path


class MultipleInvalid(Invalid):
    def __init__(self, errors=None):
        self.errors = errors[:] if errors else []

    def __repr__(self):
        return 'MultipleInvalid(%r)' % self.errors

    @property
    def msg(self):
        return self.errors[0].msg

    @property
    def path(self):
        return self.errors[0].path

    @property
    def error_message(self):
        return self.errors[0].error_message

    def add(self, error):
        self.errors.append(error)

    def __str__(self):
        return str(self.errors[0])

    def prepend(self, path):
        for error in self.errors:
            error.prepend(path)


class RequiredFieldInvalid(Invalid):
    """Required field was missing."""


class ObjectInvalid(Invalid):
    """The value we found was not an object."""


class DictInvalid(Invalid):
    """The value found was not a dict."""


class ExclusiveInvalid(Invalid):
    """More than one value found in exclusion group."""


class InclusiveInvalid(Invalid):
    """Not all values found in inclusion group."""


class SequenceTypeInvalid(Invalid):
    """The type found is not a sequence type."""


class TypeInvalid(Invalid):
    """The value was not of required type."""


class ValueInvalid(Invalid):
    """The value was found invalid by evaluation function."""


class ScalarInvalid(Invalid):
    """Scalars did not match."""


class CoerceInvalid(Invalid):
    """Impossible to coerce value to type."""


class AnyInvalid(Invalid):
    """The value did not pass any validator."""


class AllInvalid(Invalid):
    """The value did not pass all validators."""


class MatchInvalid(Invalid):
    """The value does not match the given regular expression."""


class RangeInvalid(Invalid):
    """The value is not in given range."""


class TrueInvalid(Invalid):
    """The value is not True."""


class FalseInvalid(Invalid):
    """The value is not False."""


class BooleanInvalid(Invalid):
    """The value is not a boolean."""


class UrlInvalid(Invalid):
    """The value is not a url."""


class FileInvalid(Invalid):
    """The value is not a file."""


class DirInvalid(Invalid):
    """The value is not a directory."""


class PathInvalid(Invalid):
    """The value is not a path."""


class LiteralInvalid(Invalid):
    """The literal values do not match."""


class VirtualPathComponent(str):
    def __str__(self):
        return '<' + self + '>'

    def __repr__(self):
        return self.__str__()


class Schema(object):
    """A validation schema.

    The schema is a Python tree-like structure where nodes are pattern
    matched against corresponding trees of values.

    Nodes can be values, in which case a direct comparison is used, types,
    in which case an isinstance() check is performed, or callables, which will
    validate and optionally convert the value.
    """

    _extra_to_name = {
        REMOVE_EXTRA: 'REMOVE_EXTRA',
        ALLOW_EXTRA: 'ALLOW_EXTRA',
        PREVENT_EXTRA: 'PREVENT_EXTRA',
    }

    def __init__(self, schema, required=False, extra=PREVENT_EXTRA):
        """Create a new Schema.

        :param schema: Validation schema. See :module:`voluptuous` for details.
        :param required: Keys defined in the schema must be in the data.
        :param extra: Specify how extra keys in the data are treated:
            - :const:`~voluptuous.PREVENT_EXTRA`: to disallow any undefined
              extra keys (raise ``Invalid``).
            - :const:`~voluptuous.ALLOW_EXTRA`: to include undefined extra
              keys in the output.
            - :const:`~voluptuous.REMOVE_EXTRA`: to exclude undefined extra keys
              from the output.
            - Any value other than the above defaults to
              :const:`~voluptuous.PREVENT_EXTRA`
        """
        self.schema = schema
        self.required = required
        self.extra = int(extra)  # ensure the value is an integer
        self._compiled = self._compile(schema)

    def __repr__(self):
        return "<Schema(%s, extra=%s, required=%s) object at 0x%x>" % (
            self.schema, self._extra_to_name.get(self.extra, '??'),
            self.required, id(self))

    def __call__(self, data):
        """Validate data against this schema."""
        try:
            return self._compiled([], data)
        except MultipleInvalid:
            raise
        except Invalid as e:
            raise MultipleInvalid([e])
        # return self.validate([], self.schema, data)

    def _compile(self, schema):
        if schema is Extra:
            return lambda _, v: v
        if isinstance(schema, Object):
            return self._compile_object(schema)
        if isinstance(schema, collections.Mapping):
            return self._compile_dict(schema)
        elif isinstance(schema, list):
            return self._compile_list(schema)
        elif isinstance(schema, tuple):
            return self._compile_tuple(schema)
        type_ = type(schema)
        if type_ is type:
            type_ = schema
        if type_ in (bool, int, long, str, unicode, float, complex, object,
                     list, dict, type(None)) or callable(schema):
            return _compile_scalar(schema)
        raise SchemaError('unsupported schema data type %r' %
                          type(schema).__name__)

    def _compile_mapping(self, schema, invalid_msg=None):
        """Create validator for given mapping."""
        invalid_msg = invalid_msg or 'mapping value'

        # Keys that may be required
        all_required_keys = set(key for key in schema
                                if key is not Extra
                                and ((self.required and not isinstance(key, (Optional, Remove)))
                                     or isinstance(key, Required)))

        # Keys that may have defaults
        all_default_keys = set(key for key in schema
                               if isinstance(key, Required)
                               or isinstance(key, Optional))

        _compiled_schema = {}
        for skey, svalue in iteritems(schema):
            new_key = self._compile(skey)
            new_value = self._compile(svalue)
            _compiled_schema[skey] = (new_key, new_value)

        candidates = list(_iterate_mapping_candidates(_compiled_schema))

        def validate_mapping(path, iterable, out):
            required_keys = all_required_keys.copy()
            # keeps track of all default keys that haven't been filled
            default_keys = all_default_keys.copy()
            error = None
            errors = []
            for key, value in iterable:
                key_path = path + [key]
                remove_key = False

                # compare each given key/value against all compiled key/values
                # schema key, (compiled key, compiled value)
                for skey, (ckey, cvalue) in candidates:
                    try:
                        new_key = ckey(key_path, key)
                    except Invalid as e:
                        if len(e.path) > len(key_path):
                            raise
                        if not error or len(e.path) > len(error.path):
                            error = e
                        continue
                    # Backtracking is not performed once a key is selected, so if
                    # the value is invalid we immediately throw an exception.
                    exception_errors = []
                    # check if the key is marked for removal
                    is_remove = new_key is Remove
                    try:
                        cval = cvalue(key_path, value)
                        # include if it's not marked for removal
                        if not is_remove:
                            out[new_key] = cval
                        else:
                            remove_key = True
                            continue
                    except MultipleInvalid as e:
                        exception_errors.extend(e.errors)
                    except Invalid as e:
                        exception_errors.append(e)

                    if exception_errors:
                        if is_remove or remove_key:
                            continue
                        for err in exception_errors:
                            if len(err.path) <= len(key_path):
                                err.error_type = invalid_msg
                            errors.append(err)
                        # If there is a validation error for a required
                        # key, this means that the key was provided.
                        # Discard the required key so it does not
                        # create an additional, noisy exception.
                        required_keys.discard(skey)
                        break

                    # Key and value okay, mark any Required() fields as found.
                    required_keys.discard(skey)

                    # No need for a default if it was filled
                    default_keys.discard(skey)

                    break
                else:
                    if remove_key:
                        # remove key
                        continue
                    elif self.extra == ALLOW_EXTRA:
                        out[key] = value
                    elif self.extra != REMOVE_EXTRA:
                        errors.append(Invalid('extra keys not allowed', key_path))
                    # else REMOVE_EXTRA: ignore the key so it's removed from output

            # set defaults for any that can have defaults
            for key in default_keys:
                if not isinstance(key.default, Undefined):  # if the user provides a default with the node
                    out[key.schema] = key.default()
                    if key in required_keys:
                        required_keys.discard(key)

            # for any required keys left that weren't found and don't have defaults:
            for key in required_keys:
                msg = key.msg if hasattr(key, 'msg') and key.msg else 'required key not provided'
                errors.append(RequiredFieldInvalid(msg, path + [key]))
            if errors:
                raise MultipleInvalid(errors)

            return out

        return validate_mapping

    def _compile_object(self, schema):
        """Validate an object.

        Has the same behavior as dictionary validator but work with object
        attributes.

        For example:

            >>> class Structure(object):
            ...     def __init__(self, one=None, three=None):
            ...         self.one = one
            ...         self.three = three
            ...
            >>> validate = Schema(Object({'one': 'two', 'three': 'four'}, cls=Structure))
            >>> with raises(MultipleInvalid, "not a valid value for object value @ data['one']"):
            ...   validate(Structure(one='three'))

        """
        base_validate = self._compile_mapping(
            schema, invalid_msg='object value')

        def validate_object(path, data):
            if (schema.cls is not UNDEFINED
                    and not isinstance(data, schema.cls)):
                raise ObjectInvalid('expected a {0!r}'.format(schema.cls), path)
            iterable = _iterate_object(data)
            iterable = ifilter(lambda item: item[1] is not None, iterable)
            out = base_validate(path, iterable, {})
            return type(data)(**out)

        return validate_object

    def _compile_dict(self, schema):
        """Validate a dictionary.

        A dictionary schema can contain a set of values, or at most one
        validator function/type.

        A dictionary schema will only validate a dictionary:

            >>> validate = Schema({})
            >>> with raises(MultipleInvalid, 'expected a dictionary'):
            ...   validate([])

        An invalid dictionary value:

            >>> validate = Schema({'one': 'two', 'three': 'four'})
            >>> with raises(MultipleInvalid, "not a valid value for dictionary value @ data['one']"):
            ...   validate({'one': 'three'})

        An invalid key:

            >>> with raises(MultipleInvalid, "extra keys not allowed @ data['two']"):
            ...   validate({'two': 'three'})


        Validation function, in this case the "int" type:

            >>> validate = Schema({'one': 'two', 'three': 'four', int: str})

        Valid integer input:

            >>> validate({10: 'twenty'})
            {10: 'twenty'}

        By default, a "type" in the schema (in this case "int") will be used
        purely to validate that the corresponding value is of that type. It
        will not Coerce the value:

            >>> with raises(MultipleInvalid, "extra keys not allowed @ data['10']"):
            ...   validate({'10': 'twenty'})

        Wrap them in the Coerce() function to achieve this:

            >>> validate = Schema({'one': 'two', 'three': 'four',
            ...                    Coerce(int): str})
            >>> validate({'10': 'twenty'})
            {10: 'twenty'}

        Custom message for required key

            >>> validate = Schema({Required('one', 'required'): 'two'})
            >>> with raises(MultipleInvalid, "required @ data['one']"):
            ...   validate({})

        (This is to avoid unexpected surprises.)

        Multiple errors for nested field in a dict:

        >>> validate = Schema({
        ...     'adict': {
        ...         'strfield': str,
        ...         'intfield': int
        ...     }
        ... })
        >>> try:
        ...     validate({
        ...         'adict': {
        ...             'strfield': 123,
        ...             'intfield': 'one'
        ...         }
        ...     })
        ... except MultipleInvalid as e:
        ...     print(sorted(str(i) for i in e.errors)) # doctest: +NORMALIZE_WHITESPACE
        ["expected int for dictionary value @ data['adict']['intfield']",
         "expected str for dictionary value @ data['adict']['strfield']"]

        """
        base_validate = self._compile_mapping(
            schema, invalid_msg='dictionary value')

        groups_of_exclusion = {}
        groups_of_inclusion = {}
        for node in schema:
            if isinstance(node, Exclusive):
                g = groups_of_exclusion.setdefault(node.group_of_exclusion, [])
                g.append(node)
            elif isinstance(node, Inclusive):
                g = groups_of_inclusion.setdefault(node.group_of_inclusion, [])
                g.append(node)

        def validate_dict(path, data):
            if not isinstance(data, dict):
                raise DictInvalid('expected a dictionary', path)

            errors = []
            for label, group in groups_of_exclusion.items():
                exists = False
                for exclusive in group:
                    if exclusive.schema in data:
                        if exists:
                            msg = exclusive.msg if hasattr(exclusive, 'msg') and exclusive.msg else \
                                "two or more values in the same group of exclusion '%s'" % label
                            next_path = path + [VirtualPathComponent(label)]
                            errors.append(ExclusiveInvalid(msg, next_path))
                            break
                        exists = True

            if errors:
                raise MultipleInvalid(errors)

            for label, group in groups_of_inclusion.items():
                included = [node.schema in data for node in group]
                if any(included) and not all(included):
                    msg = "some but not all values in the same group of inclusion '%s'" % label
                    for g in group:
                        if hasattr(g, 'msg') and g.msg:
                            msg = g.msg
                            break
                    next_path = path + [VirtualPathComponent(label)]
                    errors.append(InclusiveInvalid(msg, next_path))
                    break

            if errors:
                raise MultipleInvalid(errors)

            out = {}
            return base_validate(path, iteritems(data), out)

        return validate_dict

    def _compile_sequence(self, schema, seq_type):
        """Validate a sequence type.

        This is a sequence of valid values or validators tried in order.

        >>> validator = Schema(['one', 'two', int])
        >>> validator(['one'])
        ['one']
        >>> with raises(MultipleInvalid, 'expected int @ data[0]'):
        ...   validator([3.5])
        >>> validator([1])
        [1]
        """
        _compiled = [self._compile(s) for s in schema]
        seq_type_name = seq_type.__name__

        def validate_sequence(path, data):
            if not isinstance(data, seq_type):
                raise SequenceTypeInvalid('expected a %s' % seq_type_name, path)

            # Empty seq schema, allow any data.
            if not schema:
                return data

            out = []
            invalid = None
            errors = []
            index_path = UNDEFINED
            for i, value in enumerate(data):
                index_path = path + [i]
                invalid = None
                for validate in _compiled:
                    try:
                        cval = validate(index_path, value)
                        if cval is not Remove:  # do not include Remove values
                            out.append(cval)
                        break
                    except Invalid as e:
                        if len(e.path) > len(index_path):
                            raise
                        invalid = e
                else:
                    errors.append(invalid)
            if errors:
                raise MultipleInvalid(errors)
            return type(data)(out)
        return validate_sequence

    def _compile_tuple(self, schema):
        """Validate a tuple.

        A tuple is a sequence of valid values or validators tried in order.

        >>> validator = Schema(('one', 'two', int))
        >>> validator(('one',))
        ('one',)
        >>> with raises(MultipleInvalid, 'expected int @ data[0]'):
        ...   validator((3.5,))
        >>> validator((1,))
        (1,)
        """
        return self._compile_sequence(schema, tuple)

    def _compile_list(self, schema):
        """Validate a list.

        A list is a sequence of valid values or validators tried in order.

        >>> validator = Schema(['one', 'two', int])
        >>> validator(['one'])
        ['one']
        >>> with raises(MultipleInvalid, 'expected int @ data[0]'):
        ...   validator([3.5])
        >>> validator([1])
        [1]
        """
        return self._compile_sequence(schema, list)

    def extend(self, schema, required=None, extra=None):
        """Create a new `Schema` by merging this and the provided `schema`.

        Neither this `Schema` nor the provided `schema` are modified. The
        resulting `Schema` inherits the `required` and `extra` parameters of
        this, unless overridden.

        Both schemas must be dictionary-based.

        :param schema: dictionary to extend this `Schema` with
        :param required: if set, overrides `required` of this `Schema`
        :param extra: if set, overrides `extra` of this `Schema`
        """

        assert type(self.schema) == dict and type(schema) == dict, 'Both schemas must be dictionary-based'

        result = self.schema.copy()
        result.update(schema)

        result_required = (required if required is not None else self.required)
        result_extra = (extra if extra is not None else self.extra)
        return Schema(result, required=result_required, extra=result_extra)


def _compile_scalar(schema):
    """A scalar value.

    The schema can either be a value or a type.

    >>> _compile_scalar(int)([], 1)
    1
    >>> with raises(Invalid, 'expected float'):
    ...   _compile_scalar(float)([], '1')

    Callables have
    >>> _compile_scalar(lambda v: float(v))([], '1')
    1.0

    As a convenience, ValueError's are trapped:

    >>> with raises(Invalid, 'not a valid value'):
    ...   _compile_scalar(lambda v: float(v))([], 'a')
    """
    if isinstance(schema, type):
        def validate_instance(path, data):
            if isinstance(data, schema):
                return data
            else:
                msg = 'expected %s' % schema.__name__
                raise TypeInvalid(msg, path)
        return validate_instance

    if callable(schema):
        def validate_callable(path, data):
            try:
                return schema(data)
            except ValueError as e:
                raise ValueInvalid('not a valid value', path)
            except Invalid as e:
                e.prepend(path)
                raise
        return validate_callable

    def validate_value(path, data):
        if data != schema:
            raise ScalarInvalid('not a valid value', path)
        return data

    return validate_value


def _compile_itemsort():
    '''return sort function of mappings'''
    def is_extra(key_):
        return key_ is Extra

    def is_remove(key_):
        return isinstance(key_, Remove)

    def is_marker(key_):
        return isinstance(key_, Marker)

    def is_type(key_):
        return inspect.isclass(key_)

    def is_callable(key_):
        return callable(key_)

    # priority list for map sorting (in order of checking)
    # We want Extra to match last, because it's a catch-all. On the other hand,
    # Remove markers should match first (since invalid values will not
    # raise an Error, instead the validator will check if other schemas match
    # the same value).
    priority = [(1, is_remove),    # Remove highest priority after values
                (2, is_marker),    # then other Markers
                (4, is_type),      # types/classes lowest before Extra
                (3, is_callable),  # callables after markers
                (5, is_extra)]     # Extra lowest priority

    def item_priority(item_):
        key_ = item_[0]
        for i, check_ in priority:
            if check_(key_):
                return i
        # values have hightest priorities
        return 0

    return item_priority

_sort_item = _compile_itemsort()


def _iterate_mapping_candidates(schema):
    """Iterate over schema in a meaningful order."""
    # Without this, Extra might appear first in the iterator, and fail to
    # validate a key even though it's a Required that has its own validation,
    # generating a false positive.
    return sorted(iteritems(schema), key=_sort_item)


def _iterate_object(obj):
    """Return iterator over object attributes. Respect objects with
    defined __slots__.

    """
    d = {}
    try:
        d = vars(obj)
    except TypeError:
        # maybe we have named tuple here?
        if hasattr(obj, '_asdict'):
            d = obj._asdict()
    for item in iteritems(d):
        yield item
    try:
        slots = obj.__slots__
    except AttributeError:
        pass
    else:
        for key in slots:
            if key != '__dict__':
                yield (key, getattr(obj, key))
    raise StopIteration()


class Object(dict):
    """Indicate that we should work with attributes, not keys."""

    def __init__(self, schema, cls=UNDEFINED):
        self.cls = cls
        super(Object, self).__init__(schema)


class Marker(object):
    """Mark nodes for special treatment."""

    def __init__(self, schema, msg=None):
        self.schema = schema
        self._schema = Schema(schema)
        self.msg = msg

    def __call__(self, v):
        try:
            return self._schema(v)
        except Invalid as e:
            if not self.msg or len(e.path) > 1:
                raise
            raise Invalid(self.msg)

    def __str__(self):
        return str(self.schema)

    def __repr__(self):
        return repr(self.schema)

    def __lt__(self, other):
        return self.schema < other.schema


class Optional(Marker):
    """Mark a node in the schema as optional, and optionally provide a default

    >>> schema = Schema({Optional('key'): str})
    >>> schema({})
    {}
    >>> schema = Schema({Optional('key', default='value'): str})
    >>> schema({})
    {'key': 'value'}
    >>> schema = Schema({Optional('key', default=list): list})
    >>> schema({})
    {'key': []}

    If 'required' flag is set for an entire schema, optional keys aren't required

    >>> schema = Schema({
    ...    Optional('key'): str,
    ...    'key2': str
    ... }, required=True)
    >>> schema({'key2':'value'})
    {'key2': 'value'}
    """
    def __init__(self, schema, msg=None, default=UNDEFINED):
        super(Optional, self).__init__(schema, msg=msg)
        self.default = default_factory(default)


class Exclusive(Optional):
    """Mark a node in the schema as exclusive.

    Exclusive keys inherited from Optional:

    >>> schema = Schema({Exclusive('alpha', 'angles'): int, Exclusive('beta', 'angles'): int})
    >>> schema({'alpha': 30})
    {'alpha': 30}

    Keys inside a same group of exclusion cannot be together, it only makes sense for dictionaries:

    >>> with raises(MultipleInvalid, "two or more values in the same group of exclusion 'angles' @ data[<angles>]"):
    ...   schema({'alpha': 30, 'beta': 45})

    For example, API can provides multiple types of authentication, but only one works in the same time:

    >>> msg = 'Please, use only one type of authentication at the same time.'
    >>> schema = Schema({
    ... Exclusive('classic', 'auth', msg=msg):{
    ...     Required('email'): basestring,
    ...     Required('password'): basestring
    ...     },
    ... Exclusive('internal', 'auth', msg=msg):{
    ...     Required('secret_key'): basestring
    ...     },
    ... Exclusive('social', 'auth', msg=msg):{
    ...     Required('social_network'): basestring,
    ...     Required('token'): basestring
    ...     }
    ... })

    >>> with raises(MultipleInvalid, "Please, use only one type of authentication at the same time. @ data[<auth>]"):
    ...     schema({'classic': {'email': 'foo@example.com', 'password': 'bar'},
    ...             'social': {'social_network': 'barfoo', 'token': 'tEMp'}})
    """
    def __init__(self, schema, group_of_exclusion, msg=None):
        super(Exclusive, self).__init__(schema, msg=msg)
        self.group_of_exclusion = group_of_exclusion


class Inclusive(Optional):
    """ Mark a node in the schema as inclusive.

    Exclusive keys inherited from Optional:

    >>> schema = Schema({
    ...     Inclusive('filename', 'file'): str,
    ...     Inclusive('mimetype', 'file'): str
    ... })
    >>> data = {'filename': 'dog.jpg', 'mimetype': 'image/jpeg'}
    >>> data == schema(data)
    True

    Keys inside a same group of inclusive must exist together, it only makes sense for dictionaries:

    >>> with raises(MultipleInvalid, "some but not all values in the same group of inclusion 'file' @ data[<file>]"):
    ...     schema({'filename': 'dog.jpg'})

    If none of the keys in the group are present, it is accepted:

    >>> schema({})
    {}

    For example, API can return 'height' and 'width' together, but not separately.

    >>> msg = "Height and width must exist together"
    >>> schema = Schema({
    ...     Inclusive('height', 'size', msg=msg): int,
    ...     Inclusive('width', 'size', msg=msg): int
    ... })

    >>> with raises(MultipleInvalid, msg + " @ data[<size>]"):
    ...     schema({'height': 100})

    >>> with raises(MultipleInvalid, msg + " @ data[<size>]"):
    ...     schema({'width': 100})

    >>> data = {'height': 100, 'width': 100}
    >>> data == schema(data)
    True
    """

    def __init__(self, schema, group_of_inclusion, msg=None):
        super(Inclusive, self).__init__(schema, msg=msg)
        self.group_of_inclusion = group_of_inclusion


class Required(Marker):
    """Mark a node in the schema as being required, and optionally provide a default value.

    >>> schema = Schema({Required('key'): str})
    >>> with raises(MultipleInvalid, "required key not provided @ data['key']"):
    ...   schema({})

    >>> schema = Schema({Required('key', default='value'): str})
    >>> schema({})
    {'key': 'value'}
    >>> schema = Schema({Required('key', default=list): list})
    >>> schema({})
    {'key': []}
    """
    def __init__(self, schema, msg=None, default=UNDEFINED):
        super(Required, self).__init__(schema, msg=msg)
        self.default = default_factory(default)


class Remove(Marker):
    """Mark a node in the schema to be removed and excluded from the validated
    output. Keys that fail validation will not raise ``Invalid``. Instead, these
    keys will be treated as extras.

    >>> schema = Schema({str: int, Remove(int): str})
    >>> with raises(MultipleInvalid, "extra keys not allowed @ data[1]"):
    ...    schema({'keep': 1, 1: 1.0})
    >>> schema({1: 'red', 'red': 1, 2: 'green'})
    {'red': 1}
    >>> schema = Schema([int, Remove(float), Extra])
    >>> schema([1, 2, 3, 4.0, 5, 6.0, '7'])
    [1, 2, 3, 5, '7']
    """
    def __call__(self, v):
        super(Remove, self).__call__(v)
        return self.__class__

    def __repr__(self):
        return "Remove(%r)" % (self.schema,)


def Extra(_):
    """Allow keys in the data that are not present in the schema."""
    raise SchemaError('"Extra" should never be called')


# As extra() is never called there's no way to catch references to the
# deprecated object, so we just leave an alias here instead.
extra = Extra

class Msg(object):
    """Report a user-friendly message if a schema fails to validate.

    >>> validate = Schema(
    ...   Msg(['one', 'two', int],
    ...       'should be one of "one", "two" or an integer'))
    >>> with raises(MultipleInvalid, 'should be one of "one", "two" or an integer'):
    ...   validate(['three'])

    Messages are only applied to invalid direct descendants of the schema:

    >>> validate = Schema(Msg([['one', 'two', int]], 'not okay!'))
    >>> with raises(MultipleInvalid, 'expected int @ data[0][0]'):
    ...   validate([['three']])

    The type which is thrown can be overridden but needs to be a subclass of Invalid

    >>> with raises(SchemaError, 'Msg can only use subclases of Invalid as custom class'):
    ...   validate = Schema(Msg([int], 'should be int', cls=KeyError))

    If you do use a subclass of Invalid, that error will be thrown (wrapped in a MultipleInvalid)

    >>> validate = Schema(Msg([['one', 'two', int]], 'not okay!', cls=RangeInvalid))
    >>> try:
    ...  validate(['three'])
    ... except MultipleInvalid as e:
    ...   assert isinstance(e.errors[0], RangeInvalid)
    """

    def __init__(self, schema, msg, cls=None):
        if cls and not issubclass(cls, Invalid):
            raise SchemaError("Msg can only use subclases of"
                              " Invalid as custom class")
        self._schema = schema
        self.schema = Schema(schema)
        self.msg = msg
        self.cls = cls

    def __call__(self, v):
        try:
            return self.schema(v)
        except Invalid as e:
            if len(e.path) > 1:
                raise e
            else:
                raise (self.cls or Invalid)(self.msg)

    def __repr__(self):
        return 'Msg(%s, %s, cls=%s)' % (self._schema, self.msg, self.cls)


def message(default=None, cls=None):
    """Convenience decorator to allow functions to provide a message.

    Set a default message:

        >>> @message('not an integer')
        ... def isint(v):
        ...   return int(v)

        >>> validate = Schema(isint())
        >>> with raises(MultipleInvalid, 'not an integer'):
        ...   validate('a')

    The message can be overridden on a per validator basis:

        >>> validate = Schema(isint('bad'))
        >>> with raises(MultipleInvalid, 'bad'):
        ...   validate('a')

    The class thrown too:

        >>> class IntegerInvalid(Invalid): pass
        >>> validate = Schema(isint('bad', clsoverride=IntegerInvalid))
        >>> try:
        ...  validate('a')
        ... except MultipleInvalid as e:
        ...   assert isinstance(e.errors[0], IntegerInvalid)
    """
    if cls and not issubclass(cls, Invalid):
        raise SchemaError("message can only use subclases of Invalid as custom class")

    def decorator(f):
        @wraps(f)
        def check(msg=None, clsoverride=None):
            @wraps(f)
            def wrapper(*args, **kwargs):
                try:
                    return f(*args, **kwargs)
                except ValueError:
                    raise (clsoverride or cls or ValueInvalid)(msg or default or 'invalid value')
            return wrapper
        return check
    return decorator


def truth(f):
    """Convenience decorator to convert truth functions into validators.

        >>> @truth
        ... def isdir(v):
        ...   return os.path.isdir(v)
        >>> validate = Schema(isdir)
        >>> validate('/')
        '/'
        >>> with raises(MultipleInvalid, 'not a valid value'):
        ...   validate('/notavaliddir')
    """
    @wraps(f)
    def check(v):
        t = f(v)
        if not t:
            raise ValueError
        return v
    return check


class Coerce(object):
    """Coerce a value to a type.

    If the type constructor throws a ValueError or TypeError, the value
    will be marked as Invalid.

    Default behavior:

        >>> validate = Schema(Coerce(int))
        >>> with raises(MultipleInvalid, 'expected int'):
        ...   validate(None)
        >>> with raises(MultipleInvalid, 'expected int'):
        ...   validate('foo')

    With custom message:

        >>> validate = Schema(Coerce(int, "moo"))
        >>> with raises(MultipleInvalid, 'moo'):
        ...   validate('foo')
    """

    def __init__(self, type, msg=None):
        self.type = type
        self.msg = msg
        self.type_name = type.__name__

    def __call__(self, v):
        try:
            return self.type(v)
        except (ValueError, TypeError):
            msg = self.msg or ('expected %s' % self.type_name)
            raise CoerceInvalid(msg)

    def __repr__(self):
        return 'Coerce(%s, msg=%r)' % (self.type_name, self.msg)


@message('value was not true', cls=TrueInvalid)
@truth
def IsTrue(v):
    """Assert that a value is true, in the Python sense.

    >>> validate = Schema(IsTrue())

    "In the Python sense" means that implicitly false values, such as empty
    lists, dictionaries, etc. are treated as "false":

    >>> with raises(MultipleInvalid, "value was not true"):
    ...   validate([])
    >>> validate([1])
    [1]
    >>> with raises(MultipleInvalid, "value was not true"):
    ...   validate(False)

    ...and so on.

    >>> try:
    ...  validate([])
    ... except MultipleInvalid as e:
    ...   assert isinstance(e.errors[0], TrueInvalid)
    """
    return v


@message('value was not false', cls=FalseInvalid)
def IsFalse(v):
    """Assert that a value is false, in the Python sense.

    (see :func:`IsTrue` for more detail)

    >>> validate = Schema(IsFalse())
    >>> validate([])
    []
    >>> with raises(MultipleInvalid, "value was not false"):
    ...   validate(True)

    >>> try:
    ...  validate(True)
    ... except MultipleInvalid as e:
    ...   assert isinstance(e.errors[0], FalseInvalid)
    """
    if v:
        raise ValueError
    return v


@message('expected boolean', cls=BooleanInvalid)
def Boolean(v):
    """Convert human-readable boolean values to a bool.

    Accepted values are 1, true, yes, on, enable, and their negatives.
    Non-string values are cast to bool.

    >>> validate = Schema(Boolean())
    >>> validate(True)
    True
    >>> validate("1")
    True
    >>> validate("0")
    False
    >>> with raises(MultipleInvalid, "expected boolean"):
    ...   validate('moo')
    >>> try:
    ...  validate('moo')
    ... except MultipleInvalid as e:
    ...   assert isinstance(e.errors[0], BooleanInvalid)
    """
    if isinstance(v, basestring):
        v = v.lower()
        if v in ('1', 'true', 'yes', 'on', 'enable'):
            return True
        if v in ('0', 'false', 'no', 'off', 'disable'):
            return False
        raise ValueError
    return bool(v)


class Any(object):
    """Use the first validated value.

    :param msg: Message to deliver to user if validation fails.
    :param kwargs: All other keyword arguments are passed to the sub-Schema constructors.
    :returns: Return value of the first validator that passes.

    >>> validate = Schema(Any('true', 'false',
    ...                       All(Any(int, bool), Coerce(bool))))
    >>> validate('true')
    'true'
    >>> validate(1)
    True
    >>> with raises(MultipleInvalid, "not a valid value"):
    ...   validate('moo')

    msg argument is used

    >>> validate = Schema(Any(1, 2, 3, msg="Expected 1 2 or 3"))
    >>> validate(1)
    1
    >>> with raises(MultipleInvalid, "Expected 1 2 or 3"):
    ...   validate(4)
    """

    def __init__(self, *validators, **kwargs):
        self.validators = validators
        self.msg = kwargs.pop('msg', None)
        self._schemas = [Schema(val, **kwargs) for val in validators]

    def __call__(self, v):
        error = None
        for schema in self._schemas:
            try:
                return schema(v)
            except Invalid as e:
                if error is None or len(e.path) > len(error.path):
                    error = e
        else:
            if error:
                raise error if self.msg is None else AnyInvalid(self.msg)
            raise AnyInvalid(self.msg or 'no valid value found')

    def __repr__(self):
        return 'Any([%s])' % (", ".join(repr(v) for v in self.validators))


# Convenience alias
Or = Any


class All(object):
    """Value must pass all validators.

    The output of each validator is passed as input to the next.

    :param msg: Message to deliver to user if validation fails.
    :param kwargs: All other keyword arguments are passed to the sub-Schema constructors.

    >>> validate = Schema(All('10', Coerce(int)))
    >>> validate('10')
    10
    """

    def __init__(self, *validators, **kwargs):
        self.validators = validators
        self.msg = kwargs.pop('msg', None)
        self._schemas = [Schema(val, **kwargs) for val in validators]

    def __call__(self, v):
        try:
            for schema in self._schemas:
                v = schema(v)
        except Invalid as e:
            raise e if self.msg is None else AllInvalid(self.msg)
        return v

    def __repr__(self):
        return 'All(%s, msg=%r)' % (
            ", ".join(repr(v) for v in self.validators),
            self.msg
        )


# Convenience alias
And = All


class Match(object):
    """Value must be a string that matches the regular expression.

    >>> validate = Schema(Match(r'^0x[A-F0-9]+$'))
    >>> validate('0x123EF4')
    '0x123EF4'
    >>> with raises(MultipleInvalid, "does not match regular expression"):
    ...   validate('123EF4')

    >>> with raises(MultipleInvalid, 'expected string or buffer'):
    ...   validate(123)

    Pattern may also be a _compiled regular expression:

    >>> validate = Schema(Match(re.compile(r'0x[A-F0-9]+', re.I)))
    >>> validate('0x123ef4')
    '0x123ef4'
    """

    def __init__(self, pattern, msg=None):
        if isinstance(pattern, basestring):
            pattern = re.compile(pattern)
        self.pattern = pattern
        self.msg = msg

    def __call__(self, v):
        try:
            match = self.pattern.match(v)
        except TypeError:
            raise MatchInvalid("expected string or buffer")
        if not match:
            raise MatchInvalid(self.msg or 'does not match regular expression')
        return v

    def __repr__(self):
        return 'Match(%r, msg=%r)' % (self.pattern.pattern, self.msg)


class Replace(object):
    """Regex substitution.

    >>> validate = Schema(All(Replace('you', 'I'),
    ...                       Replace('hello', 'goodbye')))
    >>> validate('you say hello')
    'I say goodbye'
    """

    def __init__(self, pattern, substitution, msg=None):
        if isinstance(pattern, basestring):
            pattern = re.compile(pattern)
        self.pattern = pattern
        self.substitution = substitution
        self.msg = msg

    def __call__(self, v):
        return self.pattern.sub(self.substitution, v)

    def __repr__(self):
        return 'Replace(%r, %r, msg=%r)' % (self.pattern.pattern,
                                            self.substitution,
                                            self.msg)


def _url_validation(v):
    parsed = urlparse.urlparse(v)
    if not parsed.scheme or not parsed.netloc:
        raise UrlInvalid("must have a URL scheme and host")
    return parsed


@message('expected a Fully qualified domain name URL', cls=UrlInvalid)
def FqdnUrl(v):
    """Verify that the value is a Fully qualified domain name URL.

    >>> s = Schema(FqdnUrl())
    >>> with raises(MultipleInvalid, 'expected a Fully qualified domain name URL'):
    ...   s("http://localhost/")
    >>> s('http://w3.org')
    'http://w3.org'
    """
    try:
        parsed_url = _url_validation(v)
        if "." not in parsed_url.netloc:
            raise UrlInvalid("must have a domain name in URL")
        return v
    except:
        raise ValueError


@message('expected a URL', cls=UrlInvalid)
def Url(v):
    """Verify that the value is a URL.

    >>> s = Schema(Url())
    >>> with raises(MultipleInvalid, 'expected a URL'):
    ...   s(1)
    >>> s('http://w3.org')
    'http://w3.org'
    """
    try:
        _url_validation(v)
        return v
    except:
        raise ValueError


@message('not a file', cls=FileInvalid)
@truth
def IsFile(v):
    """Verify the file exists.

    >>> os.path.basename(IsFile()(__file__)).startswith('voluptuous.py')
    True
    >>> with raises(FileInvalid, 'not a file'):
    ...   IsFile()("random_filename_goes_here.py")
    """
    return os.path.isfile(v)


@message('not a directory', cls=DirInvalid)
@truth
def IsDir(v):
    """Verify the directory exists.

    >>> IsDir()('/')
    '/'
    """
    return os.path.isdir(v)


@message('path does not exist', cls=PathInvalid)
@truth
def PathExists(v):
    """Verify the path exists, regardless of its type.

    >>> os.path.basename(PathExists()(__file__)).startswith('voluptuous.py')
    True
    >>> with raises(Invalid, 'path does not exist'):
    ...   PathExists()("random_filename_goes_here.py")
    """
    return os.path.exists(v)


class Range(object):
    """Limit a value to a range.

    Either min or max may be omitted.
    Either min or max can be excluded from the range of accepted values.

    :raises Invalid: If the value is outside the range.

    >>> s = Schema(Range(min=1, max=10, min_included=False))
    >>> s(5)
    5
    >>> s(10)
    10
    >>> with raises(MultipleInvalid, 'value must be at most 10'):
    ...   s(20)
    >>> with raises(MultipleInvalid, 'value must be higher than 1'):
    ...   s(1)
    >>> with raises(MultipleInvalid, 'value must be lower than 10'):
    ...   Schema(Range(max=10, max_included=False))(20)
    """

    def __init__(self, min=None, max=None, min_included=True,
                 max_included=True, msg=None):
        self.min = min
        self.max = max
        self.min_included = min_included
        self.max_included = max_included
        self.msg = msg

    def __call__(self, v):
        if self.min_included:
            if self.min is not None and v < self.min:
                raise RangeInvalid(
                    self.msg or 'value must be at least %s' % self.min)
        else:
            if self.min is not None and v <= self.min:
                raise RangeInvalid(
                    self.msg or 'value must be higher than %s' % self.min)
        if self.max_included:
            if self.max is not None and v > self.max:
                raise RangeInvalid(
                    self.msg or 'value must be at most %s' % self.max)
        else:
            if self.max is not None and v >= self.max:
                raise RangeInvalid(
                    self.msg or 'value must be lower than %s' % self.max)
        return v

    def __repr__(self):
        return ('Range(min=%r, max=%r, min_included=%r,'
                ' max_included=%r, msg=%r)' % (self.min, self.max,
                                               self.min_included,
                                               self.max_included,
                                               self.msg))


class Clamp(object):
    """Clamp a value to a range.

    Either min or max may be omitted.
    >>> s = Schema(Clamp(min=0, max=1))
    >>> s(0.5)
    0.5
    >>> s(5)
    1
    >>> s(-1)
    0
    """

    def __init__(self, min=None, max=None, msg=None):
        self.min = min
        self.max = max
        self.msg = msg

    def __call__(self, v):
        if self.min is not None and v < self.min:
            v = self.min
        if self.max is not None and v > self.max:
            v = self.max
        return v

    def __repr__(self):
        return 'Clamp(min=%s, max=%s)' % (self.min, self.max)


class LengthInvalid(Invalid):
    pass


class Length(object):
    """The length of a value must be in a certain range."""

    def __init__(self, min=None, max=None, msg=None):
        self.min = min
        self.max = max
        self.msg = msg

    def __call__(self, v):
        if self.min is not None and len(v) < self.min:
            raise LengthInvalid(
                self.msg or 'length of value must be at least %s' % self.min)
        if self.max is not None and len(v) > self.max:
            raise LengthInvalid(
                self.msg or 'length of value must be at most %s' % self.max)
        return v

    def __repr__(self):
        return 'Length(min=%s, max=%s)' % (self.min, self.max)


class DatetimeInvalid(Invalid):
    """The value is not a formatted datetime string."""


class Datetime(object):
    """Validate that the value matches the datetime format."""

    DEFAULT_FORMAT = '%Y-%m-%dT%H:%M:%S.%fZ'

    def __init__(self, format=None, msg=None):
        self.format = format or self.DEFAULT_FORMAT
        self.msg = msg

    def __call__(self, v):
        try:
            datetime.datetime.strptime(v, self.format)
        except (TypeError, ValueError):
            raise DatetimeInvalid(
                self.msg or 'value does not match'
                            ' expected format %s' % self.format)
        return v

    def __repr__(self):
        return 'Datetime(format=%s)' % self.format


class InInvalid(Invalid):
    pass


class In(object):
    """Validate that a value is in a collection."""

    def __init__(self, container, msg=None):
        self.container = container
        self.msg = msg

    def __call__(self, v):
        try:
            check = v not in self.container
        except TypeError:
            check = True
        if check:
            raise InInvalid(self.msg or 'value is not allowed')
        return v

    def __repr__(self):
        return 'In(%s)' % (self.container,)


class NotInInvalid(Invalid):
    pass


class NotIn(object):
    """Validate that a value is not in a collection."""

    def __init__(self, container, msg=None):
        self.container = container
        self.msg = msg

    def __call__(self, v):
        try:
            check = v in self.container
        except TypeError:
            check = True
        if check:
            raise NotInInvalid(self.msg or 'value is not allowed')
        return v

    def __repr__(self):
        return 'NotIn(%s)' % (self.container,)


def Lower(v):
    """Transform a string to lower case.

    >>> s = Schema(Lower)
    >>> s('HI')
    'hi'
    """
    return str(v).lower()


def Upper(v):
    """Transform a string to upper case.

    >>> s = Schema(Upper)
    >>> s('hi')
    'HI'
    """
    return str(v).upper()


def Capitalize(v):
    """Capitalise a string.

    >>> s = Schema(Capitalize)
    >>> s('hello world')
    'Hello world'
    """
    return str(v).capitalize()


def Title(v):
    """Title case a string.

    >>> s = Schema(Title)
    >>> s('hello world')
    'Hello World'
    """
    return str(v).title()


def Strip(v):
    """Strip whitespace from a string.

    >>> s = Schema(Strip)
    >>> s('  hello world  ')
    'hello world'
    """
    return str(v).strip()


class DefaultTo(object):
    """Sets a value to default_value if none provided.

    >>> s = Schema(DefaultTo(42))
    >>> s(None)
    42
    >>> s = Schema(DefaultTo(list))
    >>> s(None)
    []
    """

    def __init__(self, default_value, msg=None):
        self.default_value = default_factory(default_value)
        self.msg = msg

    def __call__(self, v):
        if v is None:
            v = self.default_value()
        return v

    def __repr__(self):
        return 'DefaultTo(%s)' % (self.default_value(),)


class SetTo(object):
    """Set a value, ignoring any previous value.

    >>> s = Schema(Any(int, SetTo(42)))
    >>> s(2)
    2
    >>> s("foo")
    42
    """

    def __init__(self, value):
        self.value = default_factory(value)

    def __call__(self, v):
        return self.value()

    def __repr__(self):
        return 'SetTo(%s)' % (self.value(),)


class ExactSequenceInvalid(Invalid):
    pass


class ExactSequence(object):
    """Matches each element in a sequence against the corresponding element in
    the validators.

    :param msg: Message to deliver to user if validation fails.
    :param kwargs: All other keyword arguments are passed to the sub-Schema
        constructors.

    >>> from voluptuous import *
    >>> validate = Schema(ExactSequence([str, int, list, list]))
    >>> validate(['hourly_report', 10, [], []])
    ['hourly_report', 10, [], []]
    >>> validate(('hourly_report', 10, [], []))
    ('hourly_report', 10, [], [])
    """

    def __init__(self, validators, **kwargs):
        self.validators = validators
        self.msg = kwargs.pop('msg', None)
        self._schemas = [Schema(val, **kwargs) for val in validators]

    def __call__(self, v):
        if not isinstance(v, (list, tuple)):
            raise ExactSequenceInvalid(self.msg)
        try:
            v = type(v)(schema(x) for x, schema in zip(v, self._schemas))
        except Invalid as e:
            raise e if self.msg is None else ExactSequenceInvalid(self.msg)
        return v

    def __repr__(self):
        return 'ExactSequence([%s])' % (", ".join(repr(v)
                                                  for v in self.validators))


class Literal(object):
    def __init__(self, lit):
        self.lit = lit

    def __call__(self, value, msg=None):
        if self.lit != value:
            raise LiteralInvalid(
                msg or '%s not match for %s' % (value, self.lit)
            )
        else:
            return self.lit

    def __str__(self):
        return str(self.lit)

    def __repr__(self):
        return repr(self.lit)


class Unique(object):
    """Ensure an iterable does not contain duplicate items.

    Only iterables convertable to a set are supported (native types and
    objects with correct __eq__).

    JSON does not support set, so they need to be presented as arrays.
    Unique allows ensuring that such array does not contain dupes.

    >>> s = Schema(Unique())
    >>> s([])
    []
    >>> s([1, 2])
    [1, 2]
    >>> with raises(Invalid, 'contains duplicate items: [1]'):
    ...   s([1, 1, 2])
    >>> with raises(Invalid, "contains duplicate items: ['one']"):
    ...   s(['one', 'two', 'one'])
    >>> with raises(Invalid, regex="^contains unhashable elements: "):
    ...   s([set([1, 2]), set([3, 4])])
    >>> s('abc')
    'abc'
    >>> with raises(Invalid, regex="^contains duplicate items: "):
    ...   s('aabbc')
    """

    def __init__(self, msg=None):
        self.msg = msg

    def __call__(self, v):
        try:
            set_v = set(v)
        except TypeError as e:
            raise TypeInvalid(
                self.msg or 'contains unhashable elements: {0}'.format(e))
        if len(set_v) != len(v):
            seen = set()
            dupes = list(set(x for x in v if x in seen or seen.add(x)))
            raise Invalid(
                self.msg or 'contains duplicate items: {0}'.format(dupes))
        return v

    def __repr__(self):
        return 'Unique()'


class Set(object):
    """Convert a list into a set.

    >>> s = Schema(Set())
    >>> s([]) == set([])
    True
    >>> s([1, 2]) == set([1, 2])
    True
    >>> with raises(Invalid, regex="^cannot be presented as set: "):
    ...   s([set([1, 2]), set([3, 4])])
    """

    def __init__(self, msg=None):
        self.msg = msg

    def __call__(self, v):
        try:
            set_v = set(v)
        except Exception as e:
            raise TypeInvalid(
                self.msg or 'cannot be presented as set: {0}'.format(e))
        return set_v

    def __repr__(self):
        return 'Set()'


if __name__ == '__main__':
    import doctest
    doctest.testmod()
