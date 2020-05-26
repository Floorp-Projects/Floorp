"""
Tests for `attr.validators`.
"""

from __future__ import absolute_import, division, print_function

import pytest
import zope.interface

import attr

from attr import has
from attr import validators as validator_module
from attr._compat import TYPE
from attr.validators import (
    and_,
    deep_iterable,
    deep_mapping,
    in_,
    instance_of,
    is_callable,
    optional,
    provides,
)

from .utils import simple_attr


class TestInstanceOf(object):
    """
    Tests for `instance_of`.
    """

    def test_success(self):
        """
        Nothing happens if types match.
        """
        v = instance_of(int)
        v(None, simple_attr("test"), 42)

    def test_subclass(self):
        """
        Subclasses are accepted too.
        """
        v = instance_of(int)
        # yep, bools are a subclass of int :(
        v(None, simple_attr("test"), True)

    def test_fail(self):
        """
        Raises `TypeError` on wrong types.
        """
        v = instance_of(int)
        a = simple_attr("test")
        with pytest.raises(TypeError) as e:
            v(None, a, "42")
        assert (
            "'test' must be <{type} 'int'> (got '42' that is a <{type} "
            "'str'>).".format(type=TYPE),
            a,
            int,
            "42",
        ) == e.value.args

    def test_repr(self):
        """
        Returned validator has a useful `__repr__`.
        """
        v = instance_of(int)
        assert (
            "<instance_of validator for type <{type} 'int'>>".format(type=TYPE)
        ) == repr(v)


def always_pass(_, __, ___):
    """
    Toy validator that always passses.
    """


def always_fail(_, __, ___):
    """
    Toy validator that always fails.
    """
    0 / 0


class TestAnd(object):
    def test_success(self):
        """
        Succeeds if all wrapped validators succeed.
        """
        v = and_(instance_of(int), always_pass)

        v(None, simple_attr("test"), 42)

    def test_fail(self):
        """
        Fails if any wrapped validator fails.
        """
        v = and_(instance_of(int), always_fail)

        with pytest.raises(ZeroDivisionError):
            v(None, simple_attr("test"), 42)

    def test_sugar(self):
        """
        `and_(v1, v2, v3)` and `[v1, v2, v3]` are equivalent.
        """

        @attr.s
        class C(object):
            a1 = attr.ib("a1", validator=and_(instance_of(int)))
            a2 = attr.ib("a2", validator=[instance_of(int)])

        assert C.__attrs_attrs__[0].validator == C.__attrs_attrs__[1].validator


class IFoo(zope.interface.Interface):
    """
    An interface.
    """

    def f():
        """
        A function called f.
        """


class TestProvides(object):
    """
    Tests for `provides`.
    """

    def test_success(self):
        """
        Nothing happens if value provides requested interface.
        """

        @zope.interface.implementer(IFoo)
        class C(object):
            def f(self):
                pass

        v = provides(IFoo)
        v(None, simple_attr("x"), C())

    def test_fail(self):
        """
        Raises `TypeError` if interfaces isn't provided by value.
        """
        value = object()
        a = simple_attr("x")

        v = provides(IFoo)
        with pytest.raises(TypeError) as e:
            v(None, a, value)
        assert (
            "'x' must provide {interface!r} which {value!r} doesn't.".format(
                interface=IFoo, value=value
            ),
            a,
            IFoo,
            value,
        ) == e.value.args

    def test_repr(self):
        """
        Returned validator has a useful `__repr__`.
        """
        v = provides(IFoo)
        assert (
            "<provides validator for interface {interface!r}>".format(
                interface=IFoo
            )
        ) == repr(v)


@pytest.mark.parametrize(
    "validator", [instance_of(int), [always_pass, instance_of(int)]]
)
class TestOptional(object):
    """
    Tests for `optional`.
    """

    def test_success(self, validator):
        """
        Nothing happens if validator succeeds.
        """
        v = optional(validator)
        v(None, simple_attr("test"), 42)

    def test_success_with_none(self, validator):
        """
        Nothing happens if None.
        """
        v = optional(validator)
        v(None, simple_attr("test"), None)

    def test_fail(self, validator):
        """
        Raises `TypeError` on wrong types.
        """
        v = optional(validator)
        a = simple_attr("test")
        with pytest.raises(TypeError) as e:
            v(None, a, "42")
        assert (
            "'test' must be <{type} 'int'> (got '42' that is a <{type} "
            "'str'>).".format(type=TYPE),
            a,
            int,
            "42",
        ) == e.value.args

    def test_repr(self, validator):
        """
        Returned validator has a useful `__repr__`.
        """
        v = optional(validator)

        if isinstance(validator, list):
            repr_s = (
                "<optional validator for _AndValidator(_validators=[{func}, "
                "<instance_of validator for type <{type} 'int'>>]) or None>"
            ).format(func=repr(always_pass), type=TYPE)
        else:
            repr_s = (
                "<optional validator for <instance_of validator for type "
                "<{type} 'int'>> or None>"
            ).format(type=TYPE)

        assert repr_s == repr(v)


class TestIn_(object):
    """
    Tests for `in_`.
    """

    def test_success_with_value(self):
        """
        If the value is in our options, nothing happens.
        """
        v = in_([1, 2, 3])
        a = simple_attr("test")
        v(1, a, 3)

    def test_fail(self):
        """
        Raise ValueError if the value is outside our options.
        """
        v = in_([1, 2, 3])
        a = simple_attr("test")
        with pytest.raises(ValueError) as e:
            v(None, a, None)
        assert ("'test' must be in [1, 2, 3] (got None)",) == e.value.args

    def test_fail_with_string(self):
        """
        Raise ValueError if the value is outside our options when the
        options are specified as a string and the value is not a string.
        """
        v = in_("abc")
        a = simple_attr("test")
        with pytest.raises(ValueError) as e:
            v(None, a, None)
        assert ("'test' must be in 'abc' (got None)",) == e.value.args

    def test_repr(self):
        """
        Returned validator has a useful `__repr__`.
        """
        v = in_([3, 4, 5])
        assert (("<in_ validator with options [3, 4, 5]>")) == repr(v)


class TestDeepIterable(object):
    """
    Tests for `deep_iterable`.
    """

    def test_success_member_only(self):
        """
        If the member validator succeeds and the iterable validator is not set,
        nothing happens.
        """
        member_validator = instance_of(int)
        v = deep_iterable(member_validator)
        a = simple_attr("test")
        v(None, a, [42])

    def test_success_member_and_iterable(self):
        """
        If both the member and iterable validators succeed, nothing happens.
        """
        member_validator = instance_of(int)
        iterable_validator = instance_of(list)
        v = deep_iterable(member_validator, iterable_validator)
        a = simple_attr("test")
        v(None, a, [42])

    @pytest.mark.parametrize(
        "member_validator, iterable_validator",
        (
            (instance_of(int), 42),
            (42, instance_of(list)),
            (42, 42),
            (42, None),
        ),
    )
    def test_noncallable_validators(
        self, member_validator, iterable_validator
    ):
        """
        Raise :class:`TypeError` if any validators are not callable.
        """
        with pytest.raises(TypeError) as e:
            deep_iterable(member_validator, iterable_validator)

        e.match(r"\w* must be callable")

    def test_fail_invalid_member(self):
        """
        Raise member validator error if an invalid member is found.
        """
        member_validator = instance_of(int)
        v = deep_iterable(member_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, [42, "42"])

    def test_fail_invalid_iterable(self):
        """
        Raise iterable validator error if an invalid iterable is found.
        """
        member_validator = instance_of(int)
        iterable_validator = instance_of(tuple)
        v = deep_iterable(member_validator, iterable_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, [42])

    def test_fail_invalid_member_and_iterable(self):
        """
        Raise iterable validator error if both the iterable
        and a member are invalid.
        """
        member_validator = instance_of(int)
        iterable_validator = instance_of(tuple)
        v = deep_iterable(member_validator, iterable_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, [42, "42"])

    def test_repr_member_only(self):
        """
        Returned validator has a useful `__repr__`
        when only member validator is set.
        """
        member_validator = instance_of(int)
        member_repr = "<instance_of validator for type <{type} 'int'>>".format(
            type=TYPE
        )
        v = deep_iterable(member_validator)
        expected_repr = (
            "<deep_iterable validator for iterables of {member_repr}>"
        ).format(member_repr=member_repr)
        assert ((expected_repr)) == repr(v)

    def test_repr_member_and_iterable(self):
        """
        Returned validator has a useful `__repr__` when both member
        and iterable validators are set.
        """
        member_validator = instance_of(int)
        member_repr = "<instance_of validator for type <{type} 'int'>>".format(
            type=TYPE
        )
        iterable_validator = instance_of(list)
        iterable_repr = (
            "<instance_of validator for type <{type} 'list'>>"
        ).format(type=TYPE)
        v = deep_iterable(member_validator, iterable_validator)
        expected_repr = (
            "<deep_iterable validator for"
            " {iterable_repr} iterables of {member_repr}>"
        ).format(iterable_repr=iterable_repr, member_repr=member_repr)
        assert expected_repr == repr(v)


class TestDeepMapping(object):
    """
    Tests for `deep_mapping`.
    """

    def test_success(self):
        """
        If both the key and value validators succeed, nothing happens.
        """
        key_validator = instance_of(str)
        value_validator = instance_of(int)
        v = deep_mapping(key_validator, value_validator)
        a = simple_attr("test")
        v(None, a, {"a": 6, "b": 7})

    @pytest.mark.parametrize(
        "key_validator, value_validator, mapping_validator",
        (
            (42, instance_of(int), None),
            (instance_of(str), 42, None),
            (instance_of(str), instance_of(int), 42),
            (42, 42, None),
            (42, 42, 42),
        ),
    )
    def test_noncallable_validators(
        self, key_validator, value_validator, mapping_validator
    ):
        """
        Raise :class:`TypeError` if any validators are not callable.
        """
        with pytest.raises(TypeError) as e:
            deep_mapping(key_validator, value_validator, mapping_validator)

        e.match(r"\w* must be callable")

    def test_fail_invalid_mapping(self):
        """
        Raise :class:`TypeError` if mapping validator fails.
        """
        key_validator = instance_of(str)
        value_validator = instance_of(int)
        mapping_validator = instance_of(dict)
        v = deep_mapping(key_validator, value_validator, mapping_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, None)

    def test_fail_invalid_key(self):
        """
        Raise key validator error if an invalid key is found.
        """
        key_validator = instance_of(str)
        value_validator = instance_of(int)
        v = deep_mapping(key_validator, value_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, {"a": 6, 42: 7})

    def test_fail_invalid_member(self):
        """
        Raise key validator error if an invalid member value is found.
        """
        key_validator = instance_of(str)
        value_validator = instance_of(int)
        v = deep_mapping(key_validator, value_validator)
        a = simple_attr("test")
        with pytest.raises(TypeError):
            v(None, a, {"a": "6", "b": 7})

    def test_repr(self):
        """
        Returned validator has a useful `__repr__`.
        """
        key_validator = instance_of(str)
        key_repr = "<instance_of validator for type <{type} 'str'>>".format(
            type=TYPE
        )
        value_validator = instance_of(int)
        value_repr = "<instance_of validator for type <{type} 'int'>>".format(
            type=TYPE
        )
        v = deep_mapping(key_validator, value_validator)
        expected_repr = (
            "<deep_mapping validator for objects mapping "
            "{key_repr} to {value_repr}>"
        ).format(key_repr=key_repr, value_repr=value_repr)
        assert expected_repr == repr(v)


class TestIsCallable(object):
    """
    Tests for `is_callable`.
    """

    def test_success(self):
        """
        If the value is callable, nothing happens.
        """
        v = is_callable()
        a = simple_attr("test")
        v(None, a, isinstance)

    def test_fail(self):
        """
        Raise TypeError if the value is not callable.
        """
        v = is_callable()
        a = simple_attr("test")
        with pytest.raises(TypeError) as e:
            v(None, a, None)

        e.match("'test' must be callable")

    def test_repr(self):
        """
        Returned validator has a useful `__repr__`.
        """
        v = is_callable()
        assert "<is_callable validator>" == repr(v)


def test_hashability():
    """
    Validator classes are hashable.
    """
    for obj_name in dir(validator_module):
        obj = getattr(validator_module, obj_name)
        if not has(obj):
            continue
        hash_func = getattr(obj, "__hash__", None)
        assert hash_func is not None
        assert hash_func is not object.__hash__
