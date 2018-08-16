import collections
import inspect
import re
from functools import wraps
import sys
from contextlib import contextmanager

import itertools
from voluptuous import error as er

if sys.version_info >= (3,):
    long = int
    unicode = str
    basestring = str
    ifilter = filter

    def iteritems(d):
        return d.items()
else:
    from itertools import ifilter

    def iteritems(d):
        return d.iteritems()

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

# options for extra keys
PREVENT_EXTRA = 0  # any extra key not in schema will raise an error
ALLOW_EXTRA = 1  # extra keys not in schema will be included in output
REMOVE_EXTRA = 2  # extra keys not in schema will be excluded from output


def _isnamedtuple(obj):
    return isinstance(obj, tuple) and hasattr(obj, '_fields')


primitive_types = (str, unicode, bool, int, float)


class Undefined(object):
    def __nonzero__(self):
        return False

    def __repr__(self):
        return '...'


UNDEFINED = Undefined()


def Self():
    raise er.SchemaError('"Self" should never be called')


def default_factory(value):
    if value is UNDEFINED or callable(value):
        return value
    return lambda: value


@contextmanager
def raises(exc, msg=None, regex=None):
    try:
        yield
    except exc as e:
        if msg is not None:
            assert str(e) == msg, '%r != %r' % (str(e), msg)
        if regex is not None:
            assert re.search(regex, str(e)), '%r does not match %r' % (str(e), regex)


def Extra(_):
    """Allow keys in the data that are not present in the schema."""
    raise er.SchemaError('"Extra" should never be called')


# As extra() is never called there's no way to catch references to the
# deprecated object, so we just leave an alias here instead.
extra = Extra


class Schema(object):
    """A validation schema.

    The schema is a Python tree-like structure where nodes are pattern
    matched against corresponding trees of values.

    Nodes can be values, in which case a direct comparison is used, types,
    in which case an isinstance() check is performed, or callables, which will
    validate and optionally convert the value.

    We can equate schemas also.

    For Example:

            >>> v = Schema({Required('a'): unicode})
            >>> v1 = Schema({Required('a'): unicode})
            >>> v2 = Schema({Required('b'): unicode})
            >>> assert v == v1
            >>> assert v != v2

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

    @classmethod
    def infer(cls, data, **kwargs):
        """Create a Schema from concrete data (e.g. an API response).

        For example, this will take a dict like:

        {
            'foo': 1,
            'bar': {
                'a': True,
                'b': False
            },
            'baz': ['purple', 'monkey', 'dishwasher']
        }

        And return a Schema:

        {
            'foo': int,
            'bar': {
                'a': bool,
                'b': bool
            },
            'baz': [str]
        }

        Note: only very basic inference is supported.
        """
        def value_to_schema_type(value):
            if isinstance(value, dict):
                if len(value) == 0:
                    return dict
                return {k: value_to_schema_type(v)
                        for k, v in iteritems(value)}
            if isinstance(value, list):
                if len(value) == 0:
                    return list
                else:
                    return [value_to_schema_type(v)
                            for v in value]
            return type(value)

        return cls(value_to_schema_type(data), **kwargs)

    def __eq__(self, other):
        if not isinstance(other, Schema):
            return False
        return other.schema == self.schema

    def __ne__(self, other):
        return not (self == other)

    def __str__(self):
        return str(self.schema)

    def __repr__(self):
        return "<Schema(%s, extra=%s, required=%s) object at 0x%x>" % (
            self.schema, self._extra_to_name.get(self.extra, '??'),
            self.required, id(self))

    def __call__(self, data):
        """Validate data against this schema."""
        try:
            return self._compiled([], data)
        except er.MultipleInvalid:
            raise
        except er.Invalid as e:
            raise er.MultipleInvalid([e])
            # return self.validate([], self.schema, data)

    def _compile(self, schema):
        if schema is Extra:
            return lambda _, v: v
        if schema is Self:
            return lambda p, v: self._compiled(p, v)
        elif hasattr(schema, "__voluptuous_compile__"):
            return schema.__voluptuous_compile__(self)
        if isinstance(schema, Object):
            return self._compile_object(schema)
        if isinstance(schema, collections.Mapping):
            return self._compile_dict(schema)
        elif isinstance(schema, list):
            return self._compile_list(schema)
        elif isinstance(schema, tuple):
            return self._compile_tuple(schema)
        elif isinstance(schema, (frozenset, set)):
            return self._compile_set(schema)
        type_ = type(schema)
        if inspect.isclass(schema):
            type_ = schema
        if type_ in (bool, bytes, int, long, str, unicode, float, complex, object,
                     list, dict, type(None)) or callable(schema):
            return _compile_scalar(schema)
        raise er.SchemaError('unsupported schema data type %r' %
                             type(schema).__name__)

    def _compile_mapping(self, schema, invalid_msg=None):
        """Create validator for given mapping."""
        invalid_msg = invalid_msg or 'mapping value'

        # Keys that may be required
        all_required_keys = set(key for key in schema
                                if key is not Extra and
                                ((self.required and not isinstance(key, (Optional, Remove))) or
                                 isinstance(key, Required)))

        # Keys that may have defaults
        all_default_keys = set(key for key in schema
                               if isinstance(key, Required) or
                               isinstance(key, Optional))

        _compiled_schema = {}
        for skey, svalue in iteritems(schema):
            new_key = self._compile(skey)
            new_value = self._compile(svalue)
            _compiled_schema[skey] = (new_key, new_value)

        candidates = list(_iterate_mapping_candidates(_compiled_schema))

        # After we have the list of candidates in the correct order, we want to apply some optimization so that each
        # key in the data being validated will be matched against the relevant schema keys only.
        # No point in matching against different keys
        additional_candidates = []
        candidates_by_key = {}
        for skey, (ckey, cvalue) in candidates:
            if type(skey) in primitive_types:
                candidates_by_key.setdefault(skey, []).append((skey, (ckey, cvalue)))
            elif isinstance(skey, Marker) and type(skey.schema) in primitive_types:
                candidates_by_key.setdefault(skey.schema, []).append((skey, (ckey, cvalue)))
            else:
                # These are wildcards such as 'int', 'str', 'Remove' and others which should be applied to all keys
                additional_candidates.append((skey, (ckey, cvalue)))

        def validate_mapping(path, iterable, out):
            required_keys = all_required_keys.copy()

            # Build a map of all provided key-value pairs.
            # The type(out) is used to retain ordering in case a ordered
            # map type is provided as input.
            key_value_map = type(out)()
            for key, value in iterable:
                key_value_map[key] = value

            # Insert default values for non-existing keys.
            for key in all_default_keys:
                if not isinstance(key.default, Undefined) and \
                   key.schema not in key_value_map:
                    # A default value has been specified for this missing
                    # key, insert it.
                    key_value_map[key.schema] = key.default()

            error = None
            errors = []
            for key, value in key_value_map.items():
                key_path = path + [key]
                remove_key = False

                # Optimization. Validate against the matching key first, then fallback to the rest
                relevant_candidates = itertools.chain(candidates_by_key.get(key, []), additional_candidates)

                # compare each given key/value against all compiled key/values
                # schema key, (compiled key, compiled value)
                for skey, (ckey, cvalue) in relevant_candidates:
                    try:
                        new_key = ckey(key_path, key)
                    except er.Invalid as e:
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
                    except er.MultipleInvalid as e:
                        exception_errors.extend(e.errors)
                    except er.Invalid as e:
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

                    # Key and value okay, mark as found in case it was
                    # a Required() field.
                    required_keys.discard(skey)

                    break
                else:
                    if remove_key:
                        # remove key
                        continue
                    elif self.extra == ALLOW_EXTRA:
                        out[key] = value
                    elif self.extra != REMOVE_EXTRA:
                        errors.append(er.Invalid('extra keys not allowed', key_path))
                        # else REMOVE_EXTRA: ignore the key so it's removed from output

            # for any required keys left that weren't found and don't have defaults:
            for key in required_keys:
                msg = key.msg if hasattr(key, 'msg') and key.msg else 'required key not provided'
                errors.append(er.RequiredFieldInvalid(msg, path + [key]))
            if errors:
                raise er.MultipleInvalid(errors)

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
            >>> with raises(er.MultipleInvalid, "not a valid value for object value @ data['one']"):
            ...   validate(Structure(one='three'))

        """
        base_validate = self._compile_mapping(
            schema, invalid_msg='object value')

        def validate_object(path, data):
            if schema.cls is not UNDEFINED and not isinstance(data, schema.cls):
                raise er.ObjectInvalid('expected a {0!r}'.format(schema.cls), path)
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
            >>> with raises(er.MultipleInvalid, 'expected a dictionary'):
            ...   validate([])

        An invalid dictionary value:

            >>> validate = Schema({'one': 'two', 'three': 'four'})
            >>> with raises(er.MultipleInvalid, "not a valid value for dictionary value @ data['one']"):
            ...   validate({'one': 'three'})

        An invalid key:

            >>> with raises(er.MultipleInvalid, "extra keys not allowed @ data['two']"):
            ...   validate({'two': 'three'})


        Validation function, in this case the "int" type:

            >>> validate = Schema({'one': 'two', 'three': 'four', int: str})

        Valid integer input:

            >>> validate({10: 'twenty'})
            {10: 'twenty'}

        By default, a "type" in the schema (in this case "int") will be used
        purely to validate that the corresponding value is of that type. It
        will not Coerce the value:

            >>> with raises(er.MultipleInvalid, "extra keys not allowed @ data['10']"):
            ...   validate({'10': 'twenty'})

        Wrap them in the Coerce() function to achieve this:
            >>> from voluptuous import Coerce
            >>> validate = Schema({'one': 'two', 'three': 'four',
            ...                    Coerce(int): str})
            >>> validate({'10': 'twenty'})
            {10: 'twenty'}

        Custom message for required key

            >>> validate = Schema({Required('one', 'required'): 'two'})
            >>> with raises(er.MultipleInvalid, "required @ data['one']"):
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
        ... except er.MultipleInvalid as e:
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
                raise er.DictInvalid('expected a dictionary', path)

            errors = []
            for label, group in groups_of_exclusion.items():
                exists = False
                for exclusive in group:
                    if exclusive.schema in data:
                        if exists:
                            msg = exclusive.msg if hasattr(exclusive, 'msg') and exclusive.msg else \
                                "two or more values in the same group of exclusion '%s'" % label
                            next_path = path + [VirtualPathComponent(label)]
                            errors.append(er.ExclusiveInvalid(msg, next_path))
                            break
                        exists = True

            if errors:
                raise er.MultipleInvalid(errors)

            for label, group in groups_of_inclusion.items():
                included = [node.schema in data for node in group]
                if any(included) and not all(included):
                    msg = "some but not all values in the same group of inclusion '%s'" % label
                    for g in group:
                        if hasattr(g, 'msg') and g.msg:
                            msg = g.msg
                            break
                    next_path = path + [VirtualPathComponent(label)]
                    errors.append(er.InclusiveInvalid(msg, next_path))
                    break

            if errors:
                raise er.MultipleInvalid(errors)

            out = data.__class__()
            return base_validate(path, iteritems(data), out)

        return validate_dict

    def _compile_sequence(self, schema, seq_type):
        """Validate a sequence type.

        This is a sequence of valid values or validators tried in order.

        >>> validator = Schema(['one', 'two', int])
        >>> validator(['one'])
        ['one']
        >>> with raises(er.MultipleInvalid, 'expected int @ data[0]'):
        ...   validator([3.5])
        >>> validator([1])
        [1]
        """
        _compiled = [self._compile(s) for s in schema]
        seq_type_name = seq_type.__name__

        def validate_sequence(path, data):
            if not isinstance(data, seq_type):
                raise er.SequenceTypeInvalid('expected a %s' % seq_type_name, path)

            # Empty seq schema, allow any data.
            if not schema:
                if data:
                    raise er.MultipleInvalid([
                        er.ValueInvalid('not a valid value', [value]) for value in data
                    ])
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
                    except er.Invalid as e:
                        if len(e.path) > len(index_path):
                            raise
                        invalid = e
                else:
                    errors.append(invalid)
            if errors:
                raise er.MultipleInvalid(errors)

            if _isnamedtuple(data):
                return type(data)(*out)
            else:
                return type(data)(out)

        return validate_sequence

    def _compile_tuple(self, schema):
        """Validate a tuple.

        A tuple is a sequence of valid values or validators tried in order.

        >>> validator = Schema(('one', 'two', int))
        >>> validator(('one',))
        ('one',)
        >>> with raises(er.MultipleInvalid, 'expected int @ data[0]'):
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
        >>> with raises(er.MultipleInvalid, 'expected int @ data[0]'):
        ...   validator([3.5])
        >>> validator([1])
        [1]
        """
        return self._compile_sequence(schema, list)

    def _compile_set(self, schema):
        """Validate a set.

        A set is an unordered collection of unique elements.

        >>> validator = Schema({int})
        >>> validator(set([42])) == set([42])
        True
        >>> with raises(er.Invalid, 'expected a set'):
        ...   validator(42)
        >>> with raises(er.MultipleInvalid, 'invalid value in set'):
        ...   validator(set(['a']))
        """
        type_ = type(schema)
        type_name = type_.__name__

        def validate_set(path, data):
            if not isinstance(data, type_):
                raise er.Invalid('expected a %s' % type_name, path)

            _compiled = [self._compile(s) for s in schema]
            errors = []
            for value in data:
                for validate in _compiled:
                    try:
                        validate(path, value)
                        break
                    except er.Invalid:
                        pass
                else:
                    invalid = er.Invalid('invalid value in %s' % type_name, path)
                    errors.append(invalid)

            if errors:
                raise er.MultipleInvalid(errors)

            return data

        return validate_set

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

        # returns the key that may have been passed as arugment to Marker constructor
        def key_literal(key):
            return (key.schema if isinstance(key, Marker) else key)

        # build a map that takes the key literals to the needed objects
        # literal -> Required|Optional|literal
        result_key_map = dict((key_literal(key), key) for key in result)

        # for each item in the extension schema, replace duplicates
        # or add new keys
        for key, value in iteritems(schema):

            # if the key is already in the dictionary, we need to replace it
            # transform key to literal before checking presence
            if key_literal(key) in result_key_map:

                result_key = result_key_map[key_literal(key)]
                result_value = result[result_key]

                # if both are dictionaries, we need to extend recursively
                # create the new extended sub schema, then remove the old key and add the new one
                if type(result_value) == dict and type(value) == dict:
                    new_value = Schema(result_value).extend(value).schema
                    del result[result_key]
                    result[key] = new_value
                # one or the other or both are not sub-schemas, simple replacement is fine
                # remove old key and add new one
                else:
                    del result[result_key]
                    result[key] = value

            # key is new and can simply be added
            else:
                result[key] = value

        # recompile and send old object
        result_required = (required if required is not None else self.required)
        result_extra = (extra if extra is not None else self.extra)
        return Schema(result, required=result_required, extra=result_extra)


def _compile_scalar(schema):
    """A scalar value.

    The schema can either be a value or a type.

    >>> _compile_scalar(int)([], 1)
    1
    >>> with raises(er.Invalid, 'expected float'):
    ...   _compile_scalar(float)([], '1')

    Callables have
    >>> _compile_scalar(lambda v: float(v))([], '1')
    1.0

    As a convenience, ValueError's are trapped:

    >>> with raises(er.Invalid, 'not a valid value'):
    ...   _compile_scalar(lambda v: float(v))([], 'a')
    """
    if inspect.isclass(schema):
        def validate_instance(path, data):
            if isinstance(data, schema):
                return data
            else:
                msg = 'expected %s' % schema.__name__
                raise er.TypeInvalid(msg, path)

        return validate_instance

    if callable(schema):
        def validate_callable(path, data):
            try:
                return schema(data)
            except ValueError as e:
                raise er.ValueInvalid('not a valid value', path)
            except er.Invalid as e:
                e.prepend(path)
                raise

        return validate_callable

    def validate_value(path, data):
        if data != schema:
            raise er.ScalarInvalid('not a valid value', path)
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
    priority = [(1, is_remove),  # Remove highest priority after values
                (2, is_marker),  # then other Markers
                (4, is_type),  # types/classes lowest before Extra
                (3, is_callable),  # callables after markers
                (5, is_extra)]  # Extra lowest priority

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


class Msg(object):
    """Report a user-friendly message if a schema fails to validate.

    >>> validate = Schema(
    ...   Msg(['one', 'two', int],
    ...       'should be one of "one", "two" or an integer'))
    >>> with raises(er.MultipleInvalid, 'should be one of "one", "two" or an integer'):
    ...   validate(['three'])

    Messages are only applied to invalid direct descendants of the schema:

    >>> validate = Schema(Msg([['one', 'two', int]], 'not okay!'))
    >>> with raises(er.MultipleInvalid, 'expected int @ data[0][0]'):
    ...   validate([['three']])

    The type which is thrown can be overridden but needs to be a subclass of Invalid

    >>> with raises(er.SchemaError, 'Msg can only use subclases of Invalid as custom class'):
    ...   validate = Schema(Msg([int], 'should be int', cls=KeyError))

    If you do use a subclass of Invalid, that error will be thrown (wrapped in a MultipleInvalid)

    >>> validate = Schema(Msg([['one', 'two', int]], 'not okay!', cls=er.RangeInvalid))
    >>> try:
    ...  validate(['three'])
    ... except er.MultipleInvalid as e:
    ...   assert isinstance(e.errors[0], er.RangeInvalid)
    """

    def __init__(self, schema, msg, cls=None):
        if cls and not issubclass(cls, er.Invalid):
            raise er.SchemaError("Msg can only use subclases of"
                                 " Invalid as custom class")
        self._schema = schema
        self.schema = Schema(schema)
        self.msg = msg
        self.cls = cls

    def __call__(self, v):
        try:
            return self.schema(v)
        except er.Invalid as e:
            if len(e.path) > 1:
                raise e
            else:
                raise (self.cls or er.Invalid)(self.msg)

    def __repr__(self):
        return 'Msg(%s, %s, cls=%s)' % (self._schema, self.msg, self.cls)


class Object(dict):
    """Indicate that we should work with attributes, not keys."""

    def __init__(self, schema, cls=UNDEFINED):
        self.cls = cls
        super(Object, self).__init__(schema)


class VirtualPathComponent(str):
    def __str__(self):
        return '<' + self + '>'

    def __repr__(self):
        return self.__str__()


# Markers.py


class Marker(object):
    """Mark nodes for special treatment."""

    def __init__(self, schema_, msg=None, description=None):
        self.schema = schema_
        self._schema = Schema(schema_)
        self.msg = msg
        self.description = description

    def __call__(self, v):
        try:
            return self._schema(v)
        except er.Invalid as e:
            if not self.msg or len(e.path) > 1:
                raise
            raise er.Invalid(self.msg)

    def __str__(self):
        return str(self.schema)

    def __repr__(self):
        return repr(self.schema)

    def __lt__(self, other):
        if isinstance(other, Marker):
            return self.schema < other.schema
        return self.schema < other

    def __hash__(self):
        return hash(self.schema)

    def __eq__(self, other):
        return self.schema == other

    def __ne__(self, other):
        return not(self.schema == other)


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

    def __init__(self, schema, msg=None, default=UNDEFINED, description=None):
        super(Optional, self).__init__(schema, msg=msg,
                                       description=description)
        self.default = default_factory(default)


class Exclusive(Optional):
    """Mark a node in the schema as exclusive.

    Exclusive keys inherited from Optional:

    >>> schema = Schema({Exclusive('alpha', 'angles'): int, Exclusive('beta', 'angles'): int})
    >>> schema({'alpha': 30})
    {'alpha': 30}

    Keys inside a same group of exclusion cannot be together, it only makes sense for dictionaries:

    >>> with raises(er.MultipleInvalid, "two or more values in the same group of exclusion 'angles' @ data[<angles>]"):
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

    >>> with raises(er.MultipleInvalid, "Please, use only one type of authentication at the same time. @ data[<auth>]"):
    ...     schema({'classic': {'email': 'foo@example.com', 'password': 'bar'},
    ...             'social': {'social_network': 'barfoo', 'token': 'tEMp'}})
    """

    def __init__(self, schema, group_of_exclusion, msg=None, description=None):
        super(Exclusive, self).__init__(schema, msg=msg,
                                        description=description)
        self.group_of_exclusion = group_of_exclusion


class Inclusive(Optional):
    """ Mark a node in the schema as inclusive.

    Inclusive keys inherited from Optional:

    >>> schema = Schema({
    ...     Inclusive('filename', 'file'): str,
    ...     Inclusive('mimetype', 'file'): str
    ... })
    >>> data = {'filename': 'dog.jpg', 'mimetype': 'image/jpeg'}
    >>> data == schema(data)
    True

    Keys inside a same group of inclusive must exist together, it only makes sense for dictionaries:

    >>> with raises(er.MultipleInvalid, "some but not all values in the same group of inclusion 'file' @ data[<file>]"):
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

    >>> with raises(er.MultipleInvalid, msg + " @ data[<size>]"):
    ...     schema({'height': 100})

    >>> with raises(er.MultipleInvalid, msg + " @ data[<size>]"):
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
    >>> with raises(er.MultipleInvalid, "required key not provided @ data['key']"):
    ...   schema({})

    >>> schema = Schema({Required('key', default='value'): str})
    >>> schema({})
    {'key': 'value'}
    >>> schema = Schema({Required('key', default=list): list})
    >>> schema({})
    {'key': []}
    """

    def __init__(self, schema, msg=None, default=UNDEFINED, description=None):
        super(Required, self).__init__(schema, msg=msg,
                                       description=description)
        self.default = default_factory(default)


class Remove(Marker):
    """Mark a node in the schema to be removed and excluded from the validated
    output. Keys that fail validation will not raise ``Invalid``. Instead, these
    keys will be treated as extras.

    >>> schema = Schema({str: int, Remove(int): str})
    >>> with raises(er.MultipleInvalid, "extra keys not allowed @ data[1]"):
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

    def __hash__(self):
        return object.__hash__(self)


def message(default=None, cls=None):
    """Convenience decorator to allow functions to provide a message.

    Set a default message:

        >>> @message('not an integer')
        ... def isint(v):
        ...   return int(v)

        >>> validate = Schema(isint())
        >>> with raises(er.MultipleInvalid, 'not an integer'):
        ...   validate('a')

    The message can be overridden on a per validator basis:

        >>> validate = Schema(isint('bad'))
        >>> with raises(er.MultipleInvalid, 'bad'):
        ...   validate('a')

    The class thrown too:

        >>> class IntegerInvalid(er.Invalid): pass
        >>> validate = Schema(isint('bad', clsoverride=IntegerInvalid))
        >>> try:
        ...  validate('a')
        ... except er.MultipleInvalid as e:
        ...   assert isinstance(e.errors[0], IntegerInvalid)
    """
    if cls and not issubclass(cls, er.Invalid):
        raise er.SchemaError("message can only use subclases of Invalid as custom class")

    def decorator(f):
        @wraps(f)
        def check(msg=None, clsoverride=None):
            @wraps(f)
            def wrapper(*args, **kwargs):
                try:
                    return f(*args, **kwargs)
                except ValueError:
                    raise (clsoverride or cls or er.ValueInvalid)(msg or default or 'invalid value')

            return wrapper

        return check

    return decorator


def _args_to_dict(func, args):
    """Returns argument names as values as key-value pairs."""
    if sys.version_info >= (3, 0):
        arg_count = func.__code__.co_argcount
        arg_names = func.__code__.co_varnames[:arg_count]
    else:
        arg_count = func.func_code.co_argcount
        arg_names = func.func_code.co_varnames[:arg_count]

    arg_value_list = list(args)
    arguments = dict((arg_name, arg_value_list[i])
                     for i, arg_name in enumerate(arg_names)
                     if i < len(arg_value_list))
    return arguments


def _merge_args_with_kwargs(args_dict, kwargs_dict):
    """Merge args with kwargs."""
    ret = args_dict.copy()
    ret.update(kwargs_dict)
    return ret


def validate(*a, **kw):
    """Decorator for validating arguments of a function against a given schema.

    Set restrictions for arguments:

        >>> @validate(arg1=int, arg2=int)
        ... def foo(arg1, arg2):
        ...   return arg1 * arg2

    Set restriction for returned value:

        >>> @validate(arg=int, __return__=int)
        ... def bar(arg1):
        ...   return arg1 * 2

    """
    RETURNS_KEY = '__return__'

    def validate_schema_decorator(func):

        returns_defined = False
        returns = None

        schema_args_dict = _args_to_dict(func, a)
        schema_arguments = _merge_args_with_kwargs(schema_args_dict, kw)

        if RETURNS_KEY in schema_arguments:
            returns_defined = True
            returns = schema_arguments[RETURNS_KEY]
            del schema_arguments[RETURNS_KEY]

        input_schema = (Schema(schema_arguments, extra=ALLOW_EXTRA)
                        if len(schema_arguments) != 0 else lambda x: x)
        output_schema = Schema(returns) if returns_defined else lambda x: x

        @wraps(func)
        def func_wrapper(*args, **kwargs):
            args_dict = _args_to_dict(func, args)
            arguments = _merge_args_with_kwargs(args_dict, kwargs)
            validated_arguments = input_schema(arguments)
            output = func(**validated_arguments)
            return output_schema(output)

        return func_wrapper

    return validate_schema_decorator
