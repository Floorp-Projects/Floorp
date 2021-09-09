# coding: utf-8

"""
Unit tests of context.py.

"""

from datetime import datetime
import unittest

from pystache.context import _NOT_FOUND, _get_value, KeyNotFoundError, ContextStack
from pystache.tests.common import AssertIsMixin, AssertStringMixin, AssertExceptionMixin, Attachable

class SimpleObject(object):

    """A sample class that does not define __getitem__()."""

    def __init__(self):
        self.foo = "bar"

    def foo_callable(self):
        return "called..."


class DictLike(object):

    """A sample class that implements __getitem__() and __contains__()."""

    def __init__(self):
        self._dict = {'foo': 'bar'}
        self.fuzz = 'buzz'

    def __contains__(self, key):
        return key in self._dict

    def __getitem__(self, key):
        return self._dict[key]


class GetValueTestCase(unittest.TestCase, AssertIsMixin):

    """Test context._get_value()."""

    def assertNotFound(self, item, key):
        """
        Assert that a call to _get_value() returns _NOT_FOUND.

        """
        self.assertIs(_get_value(item, key), _NOT_FOUND)

    ### Case: the item is a dictionary.

    def test_dictionary__key_present(self):
        """
        Test getting a key from a dictionary.

        """
        item = {"foo": "bar"}
        self.assertEqual(_get_value(item, "foo"), "bar")

    def test_dictionary__callable_not_called(self):
        """
        Test that callable values are returned as-is (and in particular not called).

        """
        def foo_callable(self):
            return "bar"

        item = {"foo": foo_callable}
        self.assertNotEqual(_get_value(item, "foo"), "bar")
        self.assertTrue(_get_value(item, "foo") is foo_callable)

    def test_dictionary__key_missing(self):
        """
        Test getting a missing key from a dictionary.

        """
        item = {}
        self.assertNotFound(item, "missing")

    def test_dictionary__attributes_not_checked(self):
        """
        Test that dictionary attributes are not checked.

        """
        item = {1: 2, 3: 4}
        # I was not able to find a "public" attribute of dict that is
        # the same across Python 2/3.
        attr_name = "__len__"
        self.assertEqual(getattr(item, attr_name)(), 2)
        self.assertNotFound(item, attr_name)

    def test_dictionary__dict_subclass(self):
        """
        Test that subclasses of dict are treated as dictionaries.

        """
        class DictSubclass(dict): pass

        item = DictSubclass()
        item["foo"] = "bar"

        self.assertEqual(_get_value(item, "foo"), "bar")

    ### Case: the item is an object.

    def test_object__attribute_present(self):
        """
        Test getting an attribute from an object.

        """
        item = SimpleObject()
        self.assertEqual(_get_value(item, "foo"), "bar")

    def test_object__attribute_missing(self):
        """
        Test getting a missing attribute from an object.

        """
        item = SimpleObject()
        self.assertNotFound(item, "missing")

    def test_object__attribute_is_callable(self):
        """
        Test getting a callable attribute from an object.

        """
        item = SimpleObject()
        self.assertEqual(_get_value(item, "foo_callable"), "called...")

    def test_object__non_built_in_type(self):
        """
        Test getting an attribute from an instance of a type that isn't built-in.

        """
        item = datetime(2012, 1, 2)
        self.assertEqual(_get_value(item, "day"), 2)

    def test_object__dict_like(self):
        """
        Test getting a key from a dict-like object (an object that implements '__getitem__').

        """
        item = DictLike()
        self.assertEqual(item["foo"], "bar")
        self.assertNotFound(item, "foo")

    def test_object__property__raising_exception(self):
        """
        Test getting a property that raises an exception.

        """
        class Foo(object):

            @property
            def bar(self):
                return 1

            @property
            def baz(self):
                raise ValueError("test")

        foo = Foo()
        self.assertEqual(_get_value(foo, 'bar'), 1)
        self.assertNotFound(foo, 'missing')
        self.assertRaises(ValueError, _get_value, foo, 'baz')

    ### Case: the item is an instance of a built-in type.

    def test_built_in_type__integer(self):
        """
        Test getting from an integer.

        """
        class MyInt(int): pass

        cust_int = MyInt(10)
        pure_int = 10

        # We have to use a built-in method like __neg__ because "public"
        # attributes like "real" were not added to Python until Python 2.6,
        # when the numeric type hierarchy was added:
        #
        #   http://docs.python.org/library/numbers.html
        #
        self.assertEqual(cust_int.__neg__(), -10)
        self.assertEqual(pure_int.__neg__(), -10)

        self.assertEqual(_get_value(cust_int, '__neg__'), -10)
        self.assertNotFound(pure_int, '__neg__')

    def test_built_in_type__string(self):
        """
        Test getting from a string.

        """
        class MyStr(str): pass

        item1 = MyStr('abc')
        item2 = 'abc'

        self.assertEqual(item1.upper(), 'ABC')
        self.assertEqual(item2.upper(), 'ABC')

        self.assertEqual(_get_value(item1, 'upper'), 'ABC')
        self.assertNotFound(item2, 'upper')

    def test_built_in_type__list(self):
        """
        Test getting from a list.

        """
        class MyList(list): pass

        item1 = MyList([1, 2, 3])
        item2 = [1, 2, 3]

        self.assertEqual(item1.pop(), 3)
        self.assertEqual(item2.pop(), 3)

        self.assertEqual(_get_value(item1, 'pop'), 2)
        self.assertNotFound(item2, 'pop')


class ContextStackTestCase(unittest.TestCase, AssertIsMixin, AssertStringMixin,
                           AssertExceptionMixin):

    """
    Test the ContextStack class.

    """

    def test_init__no_elements(self):
        """
        Check that passing nothing to __init__() raises no exception.

        """
        context = ContextStack()

    def test_init__many_elements(self):
        """
        Check that passing more than two items to __init__() raises no exception.

        """
        context = ContextStack({}, {}, {})

    def test__repr(self):
        context = ContextStack()
        self.assertEqual(repr(context), 'ContextStack()')

        context = ContextStack({'foo': 'bar'})
        self.assertEqual(repr(context), "ContextStack({'foo': 'bar'},)")

        context = ContextStack({'foo': 'bar'}, {'abc': 123})
        self.assertEqual(repr(context), "ContextStack({'foo': 'bar'}, {'abc': 123})")

    def test__str(self):
        context = ContextStack()
        self.assertEqual(str(context), 'ContextStack()')

        context = ContextStack({'foo': 'bar'})
        self.assertEqual(str(context), "ContextStack({'foo': 'bar'},)")

        context = ContextStack({'foo': 'bar'}, {'abc': 123})
        self.assertEqual(str(context), "ContextStack({'foo': 'bar'}, {'abc': 123})")

    ## Test the static create() method.

    def test_create__dictionary(self):
        """
        Test passing a dictionary.

        """
        context = ContextStack.create({'foo': 'bar'})
        self.assertEqual(context.get('foo'), 'bar')

    def test_create__none(self):
        """
        Test passing None.

        """
        context = ContextStack.create({'foo': 'bar'}, None)
        self.assertEqual(context.get('foo'), 'bar')

    def test_create__object(self):
        """
        Test passing an object.

        """
        class Foo(object):
            foo = 'bar'
        context = ContextStack.create(Foo())
        self.assertEqual(context.get('foo'), 'bar')

    def test_create__context(self):
        """
        Test passing a ContextStack instance.

        """
        obj = ContextStack({'foo': 'bar'})
        context = ContextStack.create(obj)
        self.assertEqual(context.get('foo'), 'bar')

    def test_create__kwarg(self):
        """
        Test passing a keyword argument.

        """
        context = ContextStack.create(foo='bar')
        self.assertEqual(context.get('foo'), 'bar')

    def test_create__precedence_positional(self):
        """
        Test precedence of positional arguments.

        """
        context = ContextStack.create({'foo': 'bar'}, {'foo': 'buzz'})
        self.assertEqual(context.get('foo'), 'buzz')

    def test_create__precedence_keyword(self):
        """
        Test precedence of keyword arguments.

        """
        context = ContextStack.create({'foo': 'bar'}, foo='buzz')
        self.assertEqual(context.get('foo'), 'buzz')

    ## Test the get() method.

    def test_get__single_dot(self):
        """
        Test getting a single dot (".").

        """
        context = ContextStack("a", "b")
        self.assertEqual(context.get("."), "b")

    def test_get__single_dot__missing(self):
        """
        Test getting a single dot (".") with an empty context stack.

        """
        context = ContextStack()
        self.assertException(KeyNotFoundError, "Key '.' not found: empty context stack", context.get, ".")

    def test_get__key_present(self):
        """
        Test getting a key.

        """
        context = ContextStack({"foo": "bar"})
        self.assertEqual(context.get("foo"), "bar")

    def test_get__key_missing(self):
        """
        Test getting a missing key.

        """
        context = ContextStack()
        self.assertException(KeyNotFoundError, "Key 'foo' not found: first part", context.get, "foo")

    def test_get__precedence(self):
        """
        Test that get() respects the order of precedence (later items first).

        """
        context = ContextStack({"foo": "bar"}, {"foo": "buzz"})
        self.assertEqual(context.get("foo"), "buzz")

    def test_get__fallback(self):
        """
        Check that first-added stack items are queried on context misses.

        """
        context = ContextStack({"fuzz": "buzz"}, {"foo": "bar"})
        self.assertEqual(context.get("fuzz"), "buzz")

    def test_push(self):
        """
        Test push().

        """
        key = "foo"
        context = ContextStack({key: "bar"})
        self.assertEqual(context.get(key), "bar")

        context.push({key: "buzz"})
        self.assertEqual(context.get(key), "buzz")

    def test_pop(self):
        """
        Test pop().

        """
        key = "foo"
        context = ContextStack({key: "bar"}, {key: "buzz"})
        self.assertEqual(context.get(key), "buzz")

        item = context.pop()
        self.assertEqual(item, {"foo": "buzz"})
        self.assertEqual(context.get(key), "bar")

    def test_top(self):
        key = "foo"
        context = ContextStack({key: "bar"}, {key: "buzz"})
        self.assertEqual(context.get(key), "buzz")

        top = context.top()
        self.assertEqual(top, {"foo": "buzz"})
        # Make sure calling top() didn't remove the item from the stack.
        self.assertEqual(context.get(key), "buzz")

    def test_copy(self):
        key = "foo"
        original = ContextStack({key: "bar"}, {key: "buzz"})
        self.assertEqual(original.get(key), "buzz")

        new = original.copy()
        # Confirm that the copy behaves the same.
        self.assertEqual(new.get(key), "buzz")
        # Change the copy, and confirm it is changed.
        new.pop()
        self.assertEqual(new.get(key), "bar")
        # Confirm the original is unchanged.
        self.assertEqual(original.get(key), "buzz")

    def test_dot_notation__dict(self):
        name = "foo.bar"
        stack = ContextStack({"foo": {"bar": "baz"}})
        self.assertEqual(stack.get(name), "baz")

        # Works all the way down
        name = "a.b.c.d.e.f.g"
        stack = ContextStack({"a": {"b": {"c": {"d": {"e": {"f": {"g": "w00t!"}}}}}}})
        self.assertEqual(stack.get(name), "w00t!")

    def test_dot_notation__user_object(self):
        name = "foo.bar"
        stack = ContextStack({"foo": Attachable(bar="baz")})
        self.assertEqual(stack.get(name), "baz")

        # Works on multiple levels, too
        name = "a.b.c.d.e.f.g"
        A = Attachable
        stack = ContextStack({"a": A(b=A(c=A(d=A(e=A(f=A(g="w00t!"))))))})
        self.assertEqual(stack.get(name), "w00t!")

    def test_dot_notation__mixed_dict_and_obj(self):
        name = "foo.bar.baz.bak"
        stack = ContextStack({"foo": Attachable(bar={"baz": Attachable(bak=42)})})
        self.assertEqual(stack.get(name), 42)

    def test_dot_notation__missing_attr_or_key(self):
        name = "foo.bar.baz.bak"
        stack = ContextStack({"foo": {"bar": {}}})
        self.assertException(KeyNotFoundError, "Key 'foo.bar.baz.bak' not found: missing 'baz'", stack.get, name)

        stack = ContextStack({"foo": Attachable(bar=Attachable())})
        self.assertException(KeyNotFoundError, "Key 'foo.bar.baz.bak' not found: missing 'baz'", stack.get, name)

    def test_dot_notation__missing_part_terminates_search(self):
        """
        Test that dotted name resolution terminates on a later part not found.

        Check that if a later dotted name part is not found in the result from
        the former resolution, then name resolution terminates rather than
        starting the search over with the next element of the context stack.
        From the spec (interpolation section)--

          5) If any name parts were retained in step 1, each should be resolved
          against a context stack containing only the result from the former
          resolution.  If any part fails resolution, the result should be considered
          falsey, and should interpolate as the empty string.

        This test case is equivalent to the test case in the following pull
        request:

          https://github.com/mustache/spec/pull/48

        """
        stack = ContextStack({'a': {'b': 'A.B'}}, {'a': 'A'})
        self.assertEqual(stack.get('a'), 'A')
        self.assertException(KeyNotFoundError, "Key 'a.b' not found: missing 'b'", stack.get, "a.b")
        stack.pop()
        self.assertEqual(stack.get('a.b'), 'A.B')

    def test_dot_notation__autocall(self):
        name = "foo.bar.baz"

        # When any element in the path is callable, it should be automatically invoked
        stack = ContextStack({"foo": Attachable(bar=Attachable(baz=lambda: "Called!"))})
        self.assertEqual(stack.get(name), "Called!")

        class Foo(object):
            def bar(self):
                return Attachable(baz='Baz')

        stack = ContextStack({"foo": Foo()})
        self.assertEqual(stack.get(name), "Baz")
