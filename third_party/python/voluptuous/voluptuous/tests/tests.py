import copy
import collections
import sys
from nose.tools import assert_equal, assert_raises, assert_true

from voluptuous import (
    Schema, Required, Optional, Extra, Invalid, In, Remove, Literal,
    Url, MultipleInvalid, LiteralInvalid, NotIn, Match, Email,
    Replace, Range, Coerce, All, Any, Length, FqdnUrl, ALLOW_EXTRA, PREVENT_EXTRA,
    validate, ExactSequence, Equal, Unordered, Number, Maybe, Datetime, Date,
    Contains, Marker)
from voluptuous.humanize import humanize_error
from voluptuous.util import u


def test_exact_sequence():
    schema = Schema(ExactSequence([int, int]))
    try:
        schema([1, 2, 3])
    except Invalid:
        assert True
    else:
        assert False, "Did not raise Invalid"
    assert_equal(schema([1, 2]), [1, 2])


def test_required():
    """Verify that Required works."""
    schema = Schema({Required('q'): 1})
    # Can't use nose's raises (because we need to access the raised
    # exception, nor assert_raises which fails with Python 2.6.9.
    try:
        schema({})
    except Invalid as e:
        assert_equal(str(e), "required key not provided @ data['q']")
    else:
        assert False, "Did not raise Invalid"


def test_extra_with_required():
    """Verify that Required does not break Extra."""
    schema = Schema({Required('toaster'): str, Extra: object})
    r = schema({'toaster': 'blue', 'another_valid_key': 'another_valid_value'})
    assert_equal(
        r, {'toaster': 'blue', 'another_valid_key': 'another_valid_value'})


def test_iterate_candidates():
    """Verify that the order for iterating over mapping candidates is right."""
    schema = {
        "toaster": str,
        Extra: object,
    }
    # toaster should be first.
    from voluptuous.schema_builder import _iterate_mapping_candidates
    assert_equal(_iterate_mapping_candidates(schema)[0][0], 'toaster')


def test_in():
    """Verify that In works."""
    schema = Schema({"color": In(frozenset(["blue", "red", "yellow"]))})
    schema({"color": "blue"})


def test_not_in():
    """Verify that NotIn works."""
    schema = Schema({"color": NotIn(frozenset(["blue", "red", "yellow"]))})
    schema({"color": "orange"})
    try:
        schema({"color": "blue"})
    except Invalid as e:
        assert_equal(str(e), "value is not allowed for dictionary value @ data['color']")
    else:
        assert False, "Did not raise NotInInvalid"


def test_contains():
    """Verify contains validation method."""
    schema = Schema({'color': Contains('red')})
    schema({'color': ['blue', 'red', 'yellow']})
    try:
        schema({'color': ['blue', 'yellow']})
    except Invalid as e:
        assert_equal(str(e),
                     "value is not allowed for dictionary value @ data['color']")


def test_remove():
    """Verify that Remove works."""
    # remove dict keys
    schema = Schema({"weight": int,
                     Remove("color"): str,
                     Remove("amount"): int})
    out_ = schema({"weight": 10, "color": "red", "amount": 1})
    assert "color" not in out_ and "amount" not in out_

    # remove keys by type
    schema = Schema({"weight": float,
                     "amount": int,
                     # remvove str keys with int values
                     Remove(str): int,
                     # keep str keys with str values
                     str: str})
    out_ = schema({"weight": 73.4,
                   "condition": "new",
                   "amount": 5,
                   "left": 2})
    # amount should stay since it's defined
    # other string keys with int values will be removed
    assert "amount" in out_ and "left" not in out_
    # string keys with string values will stay
    assert "condition" in out_

    # remove value from list
    schema = Schema([Remove(1), int])
    out_ = schema([1, 2, 3, 4, 1, 5, 6, 1, 1, 1])
    assert_equal(out_, [2, 3, 4, 5, 6])

    # remove values from list by type
    schema = Schema([1.0, Remove(float), int])
    out_ = schema([1, 2, 1.0, 2.0, 3.0, 4])
    assert_equal(out_, [1, 2, 1.0, 4])


def test_extra_empty_errors():
    schema = Schema({'a': {Extra: object}}, required=True)
    schema({'a': {}})


def test_literal():
    """ test with Literal """

    schema = Schema([Literal({"a": 1}), Literal({"b": 1})])
    schema([{"a": 1}])
    schema([{"b": 1}])
    schema([{"a": 1}, {"b": 1}])

    try:
        schema([{"c": 1}])
    except Invalid as e:
        assert_equal(str(e), "{'c': 1} not match for {'b': 1} @ data[0]")
    else:
        assert False, "Did not raise Invalid"

    schema = Schema(Literal({"a": 1}))
    try:
        schema({"b": 1})
    except MultipleInvalid as e:
        assert_equal(str(e), "{'b': 1} not match for {'a': 1}")
        assert_equal(len(e.errors), 1)
        assert_equal(type(e.errors[0]), LiteralInvalid)
    else:
        assert False, "Did not raise Invalid"


def test_email_validation():
    """ test with valid email """
    schema = Schema({"email": Email()})
    out_ = schema({"email": "example@example.com"})

    assert 'example@example.com"', out_.get("url")


def test_email_validation_with_none():
    """ test with invalid None Email"""
    schema = Schema({"email": Email()})
    try:
        schema({"email": None})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected an Email for dictionary value @ data['email']")
    else:
        assert False, "Did not raise Invalid for None url"


def test_email_validation_with_empty_string():
    """ test with empty string Email"""
    schema = Schema({"email": Email()})
    try:
        schema({"email": ''})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected an Email for dictionary value @ data['email']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_email_validation_without_host():
    """ test with empty host name in email """
    schema = Schema({"email": Email()})
    try:
        schema({"email": 'a@.com'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected an Email for dictionary value @ data['email']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_fqdn_url_validation():
    """ test with valid fully qualified domain name url """
    schema = Schema({"url": FqdnUrl()})
    out_ = schema({"url": "http://example.com/"})

    assert 'http://example.com/', out_.get("url")


def test_fqdn_url_without_domain_name():
    """ test with invalid fully qualified domain name url """
    schema = Schema({"url": FqdnUrl()})
    try:
        schema({"url": "http://localhost/"})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a Fully qualified domain name URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for None url"


def test_fqdnurl_validation_with_none():
    """ test with invalid None FQDN url"""
    schema = Schema({"url": FqdnUrl()})
    try:
        schema({"url": None})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a Fully qualified domain name URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for None url"


def test_fqdnurl_validation_with_empty_string():
    """ test with empty string FQDN URL """
    schema = Schema({"url": FqdnUrl()})
    try:
        schema({"url": ''})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a Fully qualified domain name URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_fqdnurl_validation_without_host():
    """ test with empty host FQDN URL """
    schema = Schema({"url": FqdnUrl()})
    try:
        schema({"url": 'http://'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a Fully qualified domain name URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_url_validation():
    """ test with valid URL """
    schema = Schema({"url": Url()})
    out_ = schema({"url": "http://example.com/"})

    assert 'http://example.com/', out_.get("url")


def test_url_validation_with_none():
    """ test with invalid None url"""
    schema = Schema({"url": Url()})
    try:
        schema({"url": None})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for None url"


def test_url_validation_with_empty_string():
    """ test with empty string URL """
    schema = Schema({"url": Url()})
    try:
        schema({"url": ''})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_url_validation_without_host():
    """ test with empty host URL """
    schema = Schema({"url": Url()})
    try:
        schema({"url": 'http://'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "expected a URL for dictionary value @ data['url']")
    else:
        assert False, "Did not raise Invalid for empty string url"


def test_copy_dict_undefined():
    """ test with a copied dictionary """
    fields = {
        Required("foo"): int
    }
    copied_fields = copy.deepcopy(fields)

    schema = Schema(copied_fields)

    # This used to raise a `TypeError` because the instance of `Undefined`
    # was a copy, so object comparison would not work correctly.
    try:
        schema({"foo": "bar"})
    except Exception as e:
        assert isinstance(e, MultipleInvalid)


def test_sorting():
    """ Expect alphabetic sorting """
    foo = Required('foo')
    bar = Required('bar')
    items = [foo, bar]
    expected = [bar, foo]
    result = sorted(items)
    assert result == expected


def test_schema_extend():
    """Verify that Schema.extend copies schema keys from both."""

    base = Schema({'a': int}, required=True)
    extension = {'b': str}
    extended = base.extend(extension)

    assert base.schema == {'a': int}
    assert extension == {'b': str}
    assert extended.schema == {'a': int, 'b': str}
    assert extended.required == base.required
    assert extended.extra == base.extra


def test_schema_extend_overrides():
    """Verify that Schema.extend can override required/extra parameters."""

    base = Schema({'a': int}, required=True)
    extended = base.extend({'b': str}, required=False, extra=ALLOW_EXTRA)

    assert base.required is True
    assert base.extra == PREVENT_EXTRA
    assert extended.required is False
    assert extended.extra == ALLOW_EXTRA


def test_schema_extend_key_swap():
    """Verify that Schema.extend can replace keys, even when different markers are used"""

    base = Schema({Optional('a'): int})
    extension = {Required('a'): int}
    extended = base.extend(extension)

    assert_equal(len(base.schema), 1)
    assert_true(isinstance(list(base.schema)[0], Optional))
    assert_equal(len(extended.schema), 1)
    assert_true((list(extended.schema)[0], Required))


def test_subschema_extension():
    """Verify that Schema.extend adds and replaces keys in a subschema"""

    base = Schema({'a': {'b': int, 'c': float}})
    extension = {'d': str, 'a': {'b': str, 'e': int}}
    extended = base.extend(extension)

    assert_equal(base.schema, {'a': {'b': int, 'c': float}})
    assert_equal(extension, {'d': str, 'a': {'b': str, 'e': int}})
    assert_equal(extended.schema, {'a': {'b': str, 'c': float, 'e': int}, 'd': str})


def test_repr():
    """Verify that __repr__ returns valid Python expressions"""
    match = Match('a pattern', msg='message')
    replace = Replace('you', 'I', msg='you and I')
    range_ = Range(min=0, max=42, min_included=False,
                   max_included=False, msg='number not in range')
    coerce_ = Coerce(int, msg="moo")
    all_ = All('10', Coerce(int), msg='all msg')
    maybe_int = Maybe(int)

    assert_equal(repr(match), "Match('a pattern', msg='message')")
    assert_equal(repr(replace), "Replace('you', 'I', msg='you and I')")
    assert_equal(
        repr(range_),
        "Range(min=0, max=42, min_included=False, max_included=False, msg='number not in range')"
    )
    assert_equal(repr(coerce_), "Coerce(int, msg='moo')")
    assert_equal(repr(all_), "All('10', Coerce(int, msg=None), msg='all msg')")
    assert_equal(repr(maybe_int), "Maybe(%s)" % str(int))


def test_list_validation_messages():
    """ Make sure useful error messages are available """

    def is_even(value):
        if value % 2:
            raise Invalid('%i is not even' % value)
        return value

    schema = Schema(dict(even_numbers=[All(int, is_even)]))

    try:
        schema(dict(even_numbers=[3]))
    except Invalid as e:
        assert_equal(len(e.errors), 1, e.errors)
        assert_equal(str(e.errors[0]), "3 is not even @ data['even_numbers'][0]")
        assert_equal(str(e), "3 is not even @ data['even_numbers'][0]")
    else:
        assert False, "Did not raise Invalid"


def test_nested_multiple_validation_errors():
    """ Make sure useful error messages are available """

    def is_even(value):
        if value % 2:
            raise Invalid('%i is not even' % value)
        return value

    schema = Schema(dict(even_numbers=All([All(int, is_even)],
                                          Length(min=1))))

    try:
        schema(dict(even_numbers=[3]))
    except Invalid as e:
        assert_equal(len(e.errors), 1, e.errors)
        assert_equal(str(e.errors[0]), "3 is not even @ data['even_numbers'][0]")
        assert_equal(str(e), "3 is not even @ data['even_numbers'][0]")
    else:
        assert False, "Did not raise Invalid"


def test_humanize_error():
    data = {
        'a': 'not an int',
        'b': [123]
    }
    schema = Schema({
        'a': int,
        'b': [str]
    })
    try:
        schema(data)
    except MultipleInvalid as e:
        assert_equal(
            humanize_error(data, e),
            "expected int for dictionary value @ data['a']. Got 'not an int'\n"
            "expected str @ data['b'][0]. Got 123"
        )
    else:
        assert False, 'Did not raise MultipleInvalid'


def test_fix_157():
    s = Schema(All([Any('one', 'two', 'three')]), Length(min=1))
    assert_equal(['one'], s(['one']))
    assert_raises(MultipleInvalid, s, ['four'])


def test_range_exlcudes_nan():
    s = Schema(Range(min=0, max=10))
    assert_raises(MultipleInvalid, s, float('nan'))


def test_equal():
    s = Schema(Equal(1))
    s(1)
    assert_raises(Invalid, s, 2)
    s = Schema(Equal('foo'))
    s('foo')
    assert_raises(Invalid, s, 'bar')
    s = Schema(Equal([1, 2]))
    s([1, 2])
    assert_raises(Invalid, s, [])
    assert_raises(Invalid, s, [1, 2, 3])
    # Evaluates exactly, not through validators
    s = Schema(Equal(str))
    assert_raises(Invalid, s, 'foo')


def test_unordered():
    # Any order is OK
    s = Schema(Unordered([2, 1]))
    s([2, 1])
    s([1, 2])
    # Amount of errors is OK
    assert_raises(Invalid, s, [2, 0])
    assert_raises(MultipleInvalid, s, [0, 0])
    # Different length is NOK
    assert_raises(Invalid, s, [1])
    assert_raises(Invalid, s, [1, 2, 0])
    assert_raises(MultipleInvalid, s, [1, 2, 0, 0])
    # Other type than list or tuple is NOK
    assert_raises(Invalid, s, 'foo')
    assert_raises(Invalid, s, 10)
    # Validators are evaluated through as schemas
    s = Schema(Unordered([int, str]))
    s([1, '2'])
    s(['1', 2])
    s = Schema(Unordered([{'foo': int}, []]))
    s([{'foo': 3}, []])
    # Most accurate validators must be positioned on left
    s = Schema(Unordered([int, 3]))
    assert_raises(Invalid, s, [3, 2])
    s = Schema(Unordered([3, int]))
    s([3, 2])


def test_maybe():
    assert_raises(TypeError, Maybe, lambda x: x)

    s = Schema(Maybe(int))
    assert s(1) == 1
    assert s(None) is None

    assert_raises(Invalid, s, 'foo')


def test_empty_list_as_exact():
    s = Schema([])
    assert_raises(Invalid, s, [1])
    s([])


def test_empty_dict_as_exact():
    # {} always evaluates as {}
    s = Schema({})
    assert_raises(Invalid, s, {'extra': 1})
    s = Schema({}, extra=ALLOW_EXTRA)  # this should not be used
    assert_raises(Invalid, s, {'extra': 1})

    # {...} evaluates as Schema({...})
    s = Schema({'foo': int})
    assert_raises(Invalid, s, {'foo': 1, 'extra': 1})
    s = Schema({'foo': int}, extra=ALLOW_EXTRA)
    s({'foo': 1, 'extra': 1})

    # dict matches {} or {...}
    s = Schema(dict)
    s({'extra': 1})
    s({})
    s = Schema(dict, extra=PREVENT_EXTRA)
    s({'extra': 1})
    s({})

    # nested {} evaluate as {}
    s = Schema({
        'inner': {}
    }, extra=ALLOW_EXTRA)
    assert_raises(Invalid, s, {'inner': {'extra': 1}})
    s({})
    s = Schema({
        'inner': Schema({}, extra=ALLOW_EXTRA)
    })
    assert_raises(Invalid, s, {'inner': {'extra': 1}})
    s({})


def test_schema_decorator_match_with_args():
    @validate(int)
    def fn(arg):
        return arg

    fn(1)


def test_schema_decorator_unmatch_with_args():
    @validate(int)
    def fn(arg):
        return arg

    assert_raises(Invalid, fn, 1.0)


def test_schema_decorator_match_with_kwargs():
    @validate(arg=int)
    def fn(arg):
        return arg

    fn(1)


def test_schema_decorator_unmatch_with_kwargs():
    @validate(arg=int)
    def fn(arg):
        return arg

    assert_raises(Invalid, fn, 1.0)


def test_schema_decorator_match_return_with_args():
    @validate(int, __return__=int)
    def fn(arg):
        return arg

    fn(1)


def test_schema_decorator_unmatch_return_with_args():
    @validate(int, __return__=int)
    def fn(arg):
        return "hello"

    assert_raises(Invalid, fn, 1)


def test_schema_decorator_match_return_with_kwargs():
    @validate(arg=int, __return__=int)
    def fn(arg):
        return arg

    fn(1)


def test_schema_decorator_unmatch_return_with_kwargs():
    @validate(arg=int, __return__=int)
    def fn(arg):
        return "hello"

    assert_raises(Invalid, fn, 1)


def test_schema_decorator_return_only_match():
    @validate(__return__=int)
    def fn(arg):
        return arg

    fn(1)


def test_schema_decorator_return_only_unmatch():
    @validate(__return__=int)
    def fn(arg):
        return "hello"

    assert_raises(Invalid, fn, 1)


def test_unicode_as_key():
    if sys.version_info >= (3,):
        text_type = str
    else:
        text_type = unicode
    schema = Schema({text_type: int})
    schema({u("foobar"): 1})


def test_number_validation_with_string():
    """ test with Number with string"""
    schema = Schema({"number": Number(precision=6, scale=2)})
    try:
        schema({"number": 'teststr'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "Value must be a number enclosed with string for dictionary value @ data['number']")
    else:
        assert False, "Did not raise Invalid for String"


def test_number_validation_with_invalid_precision_invalid_scale():
    """ test with Number with invalid precision and scale"""
    schema = Schema({"number": Number(precision=6, scale=2)})
    try:
        schema({"number": '123456.712'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "Precision must be equal to 6, and Scale must be equal to 2 for dictionary value @ data['number']")
    else:
        assert False, "Did not raise Invalid for String"


def test_number_validation_with_valid_precision_scale_yield_decimal_true():
    """ test with Number with valid precision and scale"""
    schema = Schema({"number": Number(precision=6, scale=2, yield_decimal=True)})
    out_ = schema({"number": '1234.00'})
    assert_equal(float(out_.get("number")), 1234.00)


def test_number_when_precision_scale_none_yield_decimal_true():
    """ test with Number with no precision and scale"""
    schema = Schema({"number": Number(yield_decimal=True)})
    out_ = schema({"number": '12345678901234'})
    assert_equal(out_.get("number"), 12345678901234)


def test_number_when_precision_none_n_valid_scale_case1_yield_decimal_true():
    """ test with Number with no precision and valid scale case 1"""
    schema = Schema({"number": Number(scale=2, yield_decimal=True)})
    out_ = schema({"number": '123456789.34'})
    assert_equal(float(out_.get("number")), 123456789.34)


def test_number_when_precision_none_n_valid_scale_case2_yield_decimal_true():
    """ test with Number with no precision and valid scale case 2 with zero in decimal part"""
    schema = Schema({"number": Number(scale=2, yield_decimal=True)})
    out_ = schema({"number": '123456789012.00'})
    assert_equal(float(out_.get("number")), 123456789012.00)


def test_number_when_precision_none_n_invalid_scale_yield_decimal_true():
    """ test with Number with no precision and invalid scale"""
    schema = Schema({"number": Number(scale=2, yield_decimal=True)})
    try:
        schema({"number": '12345678901.234'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "Scale must be equal to 2 for dictionary value @ data['number']")
    else:
        assert False, "Did not raise Invalid for String"


def test_number_when_valid_precision_n_scale_none_yield_decimal_true():
    """ test with Number with no precision and valid scale"""
    schema = Schema({"number": Number(precision=14, yield_decimal=True)})
    out_ = schema({"number": '1234567.8901234'})
    assert_equal(float(out_.get("number")), 1234567.8901234)


def test_number_when_invalid_precision_n_scale_none_yield_decimal_true():
    """ test with Number with no precision and invalid scale"""
    schema = Schema({"number": Number(precision=14, yield_decimal=True)})
    try:
        schema({"number": '12345674.8901234'})
    except MultipleInvalid as e:
        assert_equal(str(e),
                     "Precision must be equal to 14 for dictionary value @ data['number']")
    else:
        assert False, "Did not raise Invalid for String"


def test_number_validation_with_valid_precision_scale_yield_decimal_false():
    """ test with Number with valid precision, scale and no yield_decimal"""
    schema = Schema({"number": Number(precision=6, scale=2, yield_decimal=False)})
    out_ = schema({"number": '1234.00'})
    assert_equal(out_.get("number"), '1234.00')


def test_named_tuples_validate_as_tuples():
    NT = collections.namedtuple('NT', ['a', 'b'])
    nt = NT(1, 2)
    t = (1, 2)

    Schema((int, int))(nt)
    Schema((int, int))(t)
    Schema(NT(int, int))(nt)
    Schema(NT(int, int))(t)


def test_datetime():
    schema = Schema({"datetime": Datetime()})
    schema({"datetime": "2016-10-24T14:01:57.102152Z"})
    assert_raises(MultipleInvalid, schema, {"datetime": "2016-10-24T14:01:57"})


def test_date():
    schema = Schema({"date": Date()})
    schema({"date": "2016-10-24"})
    assert_raises(MultipleInvalid, schema, {"date": "2016-10-2"})
    assert_raises(MultipleInvalid, schema, {"date": "2016-10-24Z"})


def test_ordered_dict():
    if not hasattr(collections, 'OrderedDict'):
        # collections.OrderedDict was added in Python2.7; only run if present
        return
    schema = Schema({Number(): Number()})  # x, y pairs (for interpolation or something)
    data = collections.OrderedDict([(5.0, 3.7), (24.0, 8.7), (43.0, 1.5),
                                    (62.0, 2.1), (71.5, 6.7), (90.5, 4.1),
                                    (109.0, 3.9)])
    out = schema(data)
    assert isinstance(out, collections.OrderedDict), 'Collection is no longer ordered'
    assert data.keys() == out.keys(), 'Order is not consistent'


def test_marker_hashable():
    """Verify that you can get schema keys, even if markers were used"""
    definition = {
        Required('x'): int, Optional('y'): float,
        Remove('j'): int, Remove(int): str, int: int
    }
    assert_equal(definition.get('x'), int)
    assert_equal(definition.get('y'), float)
    assert_true(Required('x') == Required('x'))
    assert_true(Required('x') != Required('y'))
    # Remove markers are not hashable
    assert_equal(definition.get('j'), None)


def test_validation_performance():
    """
    This test comes to make sure the validation complexity of dictionaries is done in a linear time.
    to achieve this a custom marker is used in the scheme that counts each time it is evaluated.
    By doing so we can determine if the validation is done in linear complexity.
    Prior to issue https://github.com/alecthomas/voluptuous/issues/259 this was exponential
    """
    num_of_keys = 1000

    schema_dict = {}
    data = {}
    data_extra_keys = {}

    counter = [0]

    class CounterMarker(Marker):
        def __call__(self, *args, **kwargs):
            counter[0] += 1
            return super(CounterMarker, self).__call__(*args, **kwargs)

    for i in range(num_of_keys):
        schema_dict[CounterMarker(str(i))] = str
        data[str(i)] = str(i)
        data_extra_keys[str(i*2)] = str(i)  # half of the keys are present, and half aren't

    schema = Schema(schema_dict, extra=ALLOW_EXTRA)

    schema(data)

    assert counter[0] <= num_of_keys, "Validation complexity is not linear! %s > %s" % (counter[0], num_of_keys)

    counter[0] = 0  # reset counter
    schema(data_extra_keys)

    assert counter[0] <= num_of_keys, "Validation complexity is not linear! %s > %s" % (counter[0], num_of_keys)
