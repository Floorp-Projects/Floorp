# coding: utf-8
"""
Module ``mock``
---------------

Wrapper to unittest.mock reducing the boilerplate when testing asyncio powered
code.

A mock can behave as a coroutine, as specified in the documentation of
:class:`~asynctest.mock.Mock`.
"""

import asyncio
import asyncio.coroutines
import contextlib
import enum
import functools
import inspect
import sys
import types
import unittest.mock


# From python 3.6, a sentinel object is used to mark coroutines (rather than
# a boolean) to prevent a mock/proxy object to return a truthy value.
# see: https://github.com/python/asyncio/commit/ea776a11f632a975ad3ebbb07d8981804aa292db
try:
    _is_coroutine = asyncio.coroutines._is_coroutine
except AttributeError:
    _is_coroutine = True


class _AsyncIterator:
    """
    Wraps an iterator in an asynchronous iterator.
    """
    def __init__(self, iterator):
        self.iterator = iterator

    def __aiter__(self):
        return self

    async def __anext__(self):
        try:
            return next(self.iterator)
        except StopIteration:
            pass
        raise StopAsyncIteration


# magic methods which must be coroutine functions
async_magic_coroutines = ("__aenter__", "__aexit__", "__anext__")
# all magic methods used in an async context
_async_magics = async_magic_coroutines + ("__aiter__", )

# We use unittest.mock.MagicProxy which works well, but it's not aware that
# we want __aexit__ to return a falsy value by default.
# We add the entry in unittest internal dict as it will not change the
# normal behavior of unittest.
unittest.mock._return_values["__aexit__"] = False


def _get_async_iter(mock):
    """
    Factory of ``__aiter__`` magic methods for a MagicMock.

    It creates a function which returns an asynchronous iterator based on the
    return value of ``mock.__aiter__``.

    Since __aiter__ used could be a coroutine in Python 3.5 and 3.6, we also
    support this case.

    See: https://www.python.org/dev/peps/pep-0525/#id23
    """
    def __aiter__():
        return_value = mock.__aiter__._mock_return_value
        if return_value is DEFAULT:
            iterator = iter([])
        else:
            iterator = iter(return_value)

        return _AsyncIterator(iterator)

    if asyncio.iscoroutinefunction(mock.__aiter__):
        return asyncio.coroutine(__aiter__)

    return __aiter__


unittest.mock._side_effect_methods["__aiter__"] = _get_async_iter


async_magic_coroutines = set(async_magic_coroutines)
_async_magics = set(_async_magics)

# This changes the behavior of unittest, but the change is minor and is
# probably better than overriding __set/get/del attr__ everywhere.
unittest.mock._all_magics |= _async_magics

def _raise(exception):
    raise exception


def _make_native_coroutine(coroutine):
    """
    Wrap a coroutine (or any function returning an awaitable) in a native
    coroutine.
    """
    if inspect.iscoroutinefunction(coroutine):
        # Nothing to do.
        return coroutine

    @functools.wraps(coroutine)
    async def wrapper(*args, **kwargs):
        return await coroutine(*args, **kwargs)

    return wrapper


def _is_started(patching):
    if isinstance(patching, _patch_dict):
        return patching._is_started
    else:
        return unittest.mock._is_started(patching)


class FakeInheritanceMeta(type):
    """
    A metaclass which recreates the original inheritance model from
    unittest.mock.

    - NonCallableMock > NonCallableMagicMock
    - NonCallable > Mock
    - Mock > MagicMock
    """
    def __init__(self, name, bases, attrs):
        attrs['__new__'] = types.MethodType(self.__new, self)
        super().__init__(name, bases, attrs)

    @staticmethod
    def __new(cls, *args, **kwargs):
        new = type(cls.__name__, (cls, ), {'__doc__': cls.__doc__})
        return object.__new__(new, *args, **kwargs)

    def __instancecheck__(cls, obj):
        # That's tricky, each type(mock) is actually a subclass of the actual
        # Mock type (see __new__)
        if super().__instancecheck__(obj):
            return True

        _type = type(obj)
        if issubclass(cls, NonCallableMock):
            if issubclass(_type, (NonCallableMagicMock, Mock, )):
                return True

        if issubclass(cls, Mock) and not issubclass(cls, CoroutineMock):
            if issubclass(_type, (MagicMock, )):
                return True

        return False


def _get_is_coroutine(self):
    return self.__dict__['_mock_is_coroutine']


def _set_is_coroutine(self, value):
    # property setters and getters are overridden by Mock(), we need to
    # update the dict to add values
    value = _is_coroutine if bool(value) else False
    self.__dict__['_mock_is_coroutine'] = value


# _mock_add_spec() is the actual private implementation in unittest.mock, we
# override it to support coroutines in the metaclass.
def _mock_add_spec(self, spec, *args, **kwargs):
    unittest.mock.NonCallableMock._mock_add_spec(self, spec, *args, **kwargs)

    _spec_coroutines = []
    for attr in dir(spec):
        if asyncio.iscoroutinefunction(getattr(spec, attr)):
            _spec_coroutines.append(attr)

    self.__dict__['_spec_coroutines'] = _spec_coroutines


def _get_child_mock(self, *args, **kwargs):
    _new_name = kwargs.get("_new_name")
    if _new_name in self.__dict__['_spec_coroutines']:
        return CoroutineMock(*args, **kwargs)

    _type = type(self)

    if issubclass(_type, MagicMock) and _new_name in async_magic_coroutines:
        klass = CoroutineMock
    elif issubclass(_type, CoroutineMock):
        klass = MagicMock
    elif not issubclass(_type, unittest.mock.CallableMixin):
        if issubclass(_type, unittest.mock.NonCallableMagicMock):
            klass = MagicMock
        elif issubclass(_type, NonCallableMock):
            klass = Mock
    else:
        klass = _type.__mro__[1]

    return klass(*args, **kwargs)


class MockMetaMixin(FakeInheritanceMeta):
    def __new__(meta, name, base, namespace):
        if not any((isinstance(baseclass, meta) for baseclass in base)):
            # this ensures that inspect.iscoroutinefunction() doesn't return
            # True when testing a mock.
            code_mock = unittest.mock.NonCallableMock(spec_set=types.CodeType)
            code_mock.co_flags = 0

            namespace.update({
                '_mock_add_spec': _mock_add_spec,
                '_get_child_mock': _get_child_mock,
                '__code__': code_mock,
            })

        return super().__new__(meta, name, base, namespace)


class IsCoroutineArgMeta(MockMetaMixin):
    def __new__(meta, name, base, namespace):
        if not any((isinstance(baseclass, meta) for baseclass in base)):
            namespace.update({
                '_asynctest_get_is_coroutine': _get_is_coroutine,
                '_asynctest_set_is_coroutine': _set_is_coroutine,
                'is_coroutine': property(_get_is_coroutine, _set_is_coroutine,
                                         doc="True if the object mocked is a coroutine"),
                '_is_coroutine': property(_get_is_coroutine),
            })

            wrapped_setattr = namespace.get("__setattr__", base[0].__setattr__)
            def __setattr__(self, attrname, value):
                if attrname == 'is_coroutine':
                    self._asynctest_set_is_coroutine(value)
                else:
                    return wrapped_setattr(self, attrname, value)

            namespace['__setattr__'] = __setattr__

        return super().__new__(meta, name, base, namespace)


class AsyncMagicMixin:
    """
    Add support for async magic methods to :class:`MagicMock` and
    :class:`NonCallableMagicMock`.

    Actually, it's a shameless copy-paste of :class:`unittest.mock.MagicMixin`:
        when added to our classes, it will just do exactly what its
        :mod:`unittest` counterpart does, but for magic methods. It adds some
        behavior but should be compatible with future additions of
        :class:`MagicMock`.
    """
    # Magic methods are invoked as type(obj).__magic__(obj), as seen in
    # PEP-343 (with) and PEP-492 (async with)
    def __init__(self, *args, **kwargs):
        self._mock_set_async_magics()  # make magic work for kwargs in init
        unittest.mock._safe_super(AsyncMagicMixin, self).__init__(*args, **kwargs)
        self._mock_set_async_magics()  # fix magic broken by upper level init

    def _mock_set_async_magics(self):
        these_magics = _async_magics

        if getattr(self, "_mock_methods", None) is not None:
            these_magics = _async_magics.intersection(self._mock_methods)
            remove_magics = _async_magics - these_magics

            for entry in remove_magics:
                if entry in type(self).__dict__:
                    # remove unneeded magic methods
                    delattr(self, entry)

        # don't overwrite existing attributes if called a second time
        these_magics = these_magics - set(type(self).__dict__)

        _type = type(self)
        for entry in these_magics:
            setattr(_type, entry, unittest.mock.MagicProxy(entry, self))

    def mock_add_spec(self, *args, **kwargs):
        unittest.mock.MagicMock.mock_add_spec(self, *args, **kwargs)
        self._mock_set_async_magics()

    def __setattr__(self, name, value):
        _mock_methods = getattr(self, '_mock_methods', None)
        if _mock_methods is None or name in _mock_methods:
            if name in _async_magics:
                if not unittest.mock._is_instance_mock(value):
                    setattr(type(self), name,
                            unittest.mock._get_method(name, value))
                    original = value

                    def value(*args, **kwargs):
                        return original(self, *args, **kwargs)
                else:
                    unittest.mock._check_and_set_parent(self, value, None, name)
                    setattr(type(self), name, value)
                    self._mock_children[name] = value

                return object.__setattr__(self, name, value)

        unittest.mock._safe_super(AsyncMagicMixin, self).__setattr__(name, value)


# Notes about unittest.mock:
#  - MagicMock > Mock > NonCallableMock (where ">" means inherits from)
#  - when a mock instance is created, a new class (type) is created
#    dynamically,
#  - we *must* use magic or object's internals when we want to add our own
#    properties, and often override __getattr__/__setattr__ which are used
#    in unittest.mock.NonCallableMock.
class NonCallableMock(unittest.mock.NonCallableMock,
                      metaclass=IsCoroutineArgMeta):
    """
    Enhance :class:`unittest.mock.NonCallableMock` with features allowing to
    mock a coroutine function.

    If ``is_coroutine`` is set to ``True``, the :class:`NonCallableMock`
    object will behave so :func:`asyncio.iscoroutinefunction` will return
    ``True`` with ``mock`` as parameter.

    If ``spec`` or ``spec_set`` is defined and an attribute is get,
    :class:`~asynctest.CoroutineMock` is returned instead of
    :class:`~asynctest.Mock` when the matching spec attribute is a coroutine
    function.

    The test author can also specify a wrapped object with ``wraps``. In this
    case, the :class:`~asynctest.Mock` object behavior is the same as with an
    :class:`unittest.mock.Mock` object: the wrapped object may have methods
    defined as coroutine functions.

    See :class:`unittest.mock.NonCallableMock`
    """
    def __init__(self, spec=None, wraps=None, name=None, spec_set=None,
                 is_coroutine=None, parent=None, **kwargs):
        super().__init__(spec=spec, wraps=wraps, name=name, spec_set=spec_set,
                         parent=parent, **kwargs)

        self._asynctest_set_is_coroutine(is_coroutine)


class NonCallableMagicMock(AsyncMagicMixin, unittest.mock.NonCallableMagicMock,
                           metaclass=IsCoroutineArgMeta):
    """
    A version of :class:`~asynctest.MagicMock` that isn't callable.
    """
    def __init__(self, spec=None, wraps=None, name=None, spec_set=None,
                 is_coroutine=None, parent=None, **kwargs):

        super().__init__(spec=spec, wraps=wraps, name=name, spec_set=spec_set,
                         parent=parent, **kwargs)

        self._asynctest_set_is_coroutine(is_coroutine)


class Mock(unittest.mock.Mock, metaclass=MockMetaMixin):
    """
    Enhance :class:`unittest.mock.Mock` so it returns
    a :class:`~asynctest.CoroutineMock` object instead of
    a :class:`~asynctest.Mock` object where a method on a ``spec`` or
    ``spec_set`` object is a coroutine.

    For instance:

    >>> class Foo:
    ...     @asyncio.coroutine
    ...     def foo(self):
    ...         pass
    ...
    ...     def bar(self):
    ...         pass

    >>> type(asynctest.mock.Mock(Foo()).foo)
    <class 'asynctest.mock.CoroutineMock'>

    >>> type(asynctest.mock.Mock(Foo()).bar)
    <class 'asynctest.mock.Mock'>

    The test author can also specify a wrapped object with ``wraps``. In this
    case, the :class:`~asynctest.Mock` object behavior is the same as with an
    :class:`unittest.mock.Mock` object: the wrapped object may have methods
    defined as coroutine functions.

    If you want to mock a coroutine function, use :class:`CoroutineMock`
    instead.

    See :class:`~asynctest.NonCallableMock` for details about :mod:`asynctest`
    features, and :mod:`unittest.mock` for the comprehensive documentation
    about mocking.
    """


class MagicMock(AsyncMagicMixin, unittest.mock.MagicMock,
                metaclass=MockMetaMixin):
    """
    Enhance :class:`unittest.mock.MagicMock` so it returns
    a :class:`~asynctest.CoroutineMock` object instead of
    a :class:`~asynctest.Mock` object where a method on a ``spec`` or
    ``spec_set`` object is a coroutine.

    If you want to mock a coroutine function, use :class:`CoroutineMock`
    instead.

    :class:`MagicMock` allows to mock ``__aenter__``, ``__aexit__``,
    ``__aiter__`` and ``__anext__``.

    When mocking an asynchronous iterator, you can set the
    ``return_value`` of ``__aiter__`` to an iterable to define the list of
    values to be returned during iteration.

    You can not mock ``__await__``. If you want to mock an object implementing
    __await__, :class:`CoroutineMock` will likely be sufficient.

    see :class:`~asynctest.Mock`.

    .. versionadded:: 0.11

        support of asynchronous iterators and asynchronous context managers.
    """


class _AwaitEvent:
    def __init__(self, mock):
        self._mock = mock
        self._condition = None

    @asyncio.coroutine
    def wait(self, skip=0):
        """
        Wait for await.

        :param skip: How many awaits will be skipped.
                     As a result, the mock should be awaited at least
                     ``skip + 1`` times.
        """
        def predicate(mock):
            return mock.await_count > skip

        return (yield from self.wait_for(predicate))

    @asyncio.coroutine
    def wait_next(self, skip=0):
        """
        Wait for the next await.

        Unlike :meth:`wait` that counts any await, mock has to be awaited once
        more, disregarding to the current
        :attr:`asynctest.CoroutineMock.await_count`.

        :param skip: How many awaits will be skipped.
                     As a result, the mock should be awaited at least
                     ``skip + 1`` more times.
        """
        await_count = self._mock.await_count

        def predicate(mock):
            return mock.await_count > await_count + skip

        return (yield from self.wait_for(predicate))

    @asyncio.coroutine
    def wait_for(self, predicate):
        """
        Wait for a given predicate to become True.

        :param predicate: A callable that receives mock which result
                          will be interpreted as a boolean value.
                          The final predicate value is the return value.
        """
        condition = self._get_condition()

        try:
            yield from condition.acquire()

            def _predicate():
                return predicate(self._mock)

            return (yield from condition.wait_for(_predicate))
        finally:
            condition.release()

    @asyncio.coroutine
    def _notify(self):
        condition = self._get_condition()

        try:
            yield from condition.acquire()
            condition.notify_all()
        finally:
            condition.release()

    def _get_condition(self):
        """
        Creation of condition is delayed, to minimize the change of using the
        wrong loop.

        A user may create a mock with _AwaitEvent before selecting the
        execution loop.  Requiring a user to delay creation is error-prone and
        inflexible. Instead, condition is created when user actually starts to
        use the mock.
        """
        # No synchronization is needed:
        #   - asyncio is thread unsafe
        #   - there are no awaits here, method will be executed without
        #   switching asyncio context.
        if self._condition is None:
            self._condition = asyncio.Condition()

        return self._condition

    def __bool__(self):
        return self._mock.await_count != 0


class CoroutineMock(Mock):
    """
    Enhance :class:`~asynctest.mock.Mock` with features allowing to mock
    a coroutine function.

    The :class:`~asynctest.CoroutineMock` object will behave so the object is
    recognized as coroutine function, and the result of a call as a coroutine:

    >>> mock = CoroutineMock()
    >>> asyncio.iscoroutinefunction(mock)
    True
    >>> asyncio.iscoroutine(mock())
    True


    The result of ``mock()`` is a coroutine which will have the outcome of
    ``side_effect`` or ``return_value``:

    - if ``side_effect`` is a function, the coroutine will return the result
      of that function,
    - if ``side_effect`` is an exception, the coroutine will raise the
      exception,
    - if ``side_effect`` is an iterable, the coroutine will return the next
      value of the iterable, however, if the sequence of result is exhausted,
      ``StopIteration`` is raised immediately,
    - if ``side_effect`` is not defined, the coroutine will return the value
      defined by ``return_value``, hence, by default, the coroutine returns
      a new :class:`~asynctest.CoroutineMock` object.

    If the outcome of ``side_effect`` or ``return_value`` is a coroutine, the
    mock coroutine obtained when the mock object is called will be this
    coroutine itself (and not a coroutine returning a coroutine).

    The test author can also specify a wrapped object with ``wraps``. In this
    case, the :class:`~asynctest.Mock` object behavior is the same as with an
    :class:`unittest.mock.Mock` object: the wrapped object may have methods
    defined as coroutine functions.
    """
    #: Property which is set when the mock is awaited. Its ``wait`` and
    #: ``wait_next`` coroutine methods can be used to synchronize execution.
    #:
    #: .. versionadded:: 0.12
    awaited = unittest.mock._delegating_property('awaited')
    #: Number of times the mock has been awaited.
    #:
    #: .. versionadded:: 0.12
    await_count = unittest.mock._delegating_property('await_count')
    await_args = unittest.mock._delegating_property('await_args')
    await_args_list = unittest.mock._delegating_property('await_args_list')

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # asyncio.iscoroutinefunction() checks this property to say if an
        # object is a coroutine
        # It is set through __dict__ because when spec_set is True, this
        # attribute is likely undefined.
        self.__dict__['_is_coroutine'] = _is_coroutine
        self.__dict__['_mock_awaited'] = _AwaitEvent(self)
        self.__dict__['_mock_await_count'] = 0
        self.__dict__['_mock_await_args'] = None
        self.__dict__['_mock_await_args_list'] = unittest.mock._CallList()

    def _mock_call(_mock_self, *args, **kwargs):
        try:
            result = super()._mock_call(*args, **kwargs)
        except StopIteration as e:
            side_effect = _mock_self.side_effect
            if side_effect is not None and not callable(side_effect):
                raise

            result = asyncio.coroutine(_raise)(e)
        except BaseException as e:
            result = asyncio.coroutine(_raise)(e)

        _call = _mock_self.call_args

        @asyncio.coroutine
        def proxy():
            try:
                if inspect.isawaitable(result):
                    return (yield from result)
                else:
                    return result
            finally:
                _mock_self.await_count += 1
                _mock_self.await_args = _call
                _mock_self.await_args_list.append(_call)
                yield from _mock_self.awaited._notify()

        return proxy()

    def assert_awaited(_mock_self):
        """
        Assert that the mock was awaited at least once.

        .. versionadded:: 0.12
        """
        self = _mock_self
        if self.await_count == 0:
            msg = ("Expected '%s' to have been awaited." %
                   self._mock_name or 'mock')
            raise AssertionError(msg)

    def assert_awaited_once(_mock_self, *args, **kwargs):
        """
        Assert that the mock was awaited exactly once.

        .. versionadded:: 0.12
        """
        self = _mock_self
        if not self.await_count == 1:
            msg = ("Expected '%s' to have been awaited once. Awaited %s times." %
                   (self._mock_name or 'mock', self.await_count))
            raise AssertionError(msg)

    def assert_awaited_with(_mock_self, *args, **kwargs):
        """
        Assert that the last await was with the specified arguments.

        .. versionadded:: 0.12
        """
        self = _mock_self
        if self.await_args is None:
            expected = self._format_mock_call_signature(args, kwargs)
            raise AssertionError('Expected await: %s\nNot awaited' % (expected,))

        def _error_message():
            msg = self._format_mock_failure_message(args, kwargs)
            return msg

        expected = self._call_matcher((args, kwargs))
        actual = self._call_matcher(self.await_args)
        if expected != actual:
            cause = expected if isinstance(expected, Exception) else None
            raise AssertionError(_error_message()) from cause

    def assert_awaited_once_with(_mock_self, *args, **kwargs):
        """
        Assert that the mock was awaited exactly once and with the specified arguments.

        .. versionadded:: 0.12
        """
        self = _mock_self
        if not self.await_count == 1:
            msg = ("Expected '%s' to be awaited once. Awaited %s times." %
                   (self._mock_name or 'mock', self.await_count))
            raise AssertionError(msg)
        return self.assert_awaited_with(*args, **kwargs)

    def assert_any_await(_mock_self, *args, **kwargs):
        """
        Assert the mock has ever been awaited with the specified arguments.

        .. versionadded:: 0.12
        """
        self = _mock_self
        expected = self._call_matcher((args, kwargs))
        actual = [self._call_matcher(c) for c in self.await_args_list]
        if expected not in actual:
            cause = expected if isinstance(expected, Exception) else None
            expected_string = self._format_mock_call_signature(args, kwargs)
            raise AssertionError(
                '%s await not found' % expected_string
            ) from cause

    def assert_has_awaits(_mock_self, calls, any_order=False):
        """
        Assert the mock has been awaited with the specified calls.
        The :attr:`await_args_list` list is checked for the awaits.

        If `any_order` is False (the default) then the awaits must be
        sequential. There can be extra calls before or after the
        specified awaits.

        If `any_order` is True then the awaits can be in any order, but
        they must all appear in :attr:`await_args_list`.

        .. versionadded:: 0.12
        """
        self = _mock_self
        expected = [self._call_matcher(c) for c in calls]
        cause = expected if isinstance(expected, Exception) else None
        all_awaits = unittest.mock._CallList(self._call_matcher(c) for c in self.await_args_list)
        if not any_order:
            if expected not in all_awaits:
                raise AssertionError(
                    'Awaits not found.\nExpected: %r\n'
                    'Actual: %r' % (unittest.mock._CallList(calls), self.await_args_list)
                ) from cause
            return

        all_awaits = list(all_awaits)

        not_found = []
        for kall in expected:
            try:
                all_awaits.remove(kall)
            except ValueError:
                not_found.append(kall)
        if not_found:
            raise AssertionError(
                '%r not all found in await list' % (tuple(not_found),)
            ) from cause

    def assert_not_awaited(_mock_self):
        """
        Assert that the mock was never awaited.

        .. versionadded:: 0.12
        """
        self = _mock_self
        if self.await_count != 0:
            msg = ("Expected '%s' to not have been awaited. Awaited %s times." %
                   (self._mock_name or 'mock', self.await_count))
            raise AssertionError(msg)

    def reset_mock(self, *args, **kwargs):
        """
        See :func:`unittest.mock.Mock.reset_mock()`
        """
        super().reset_mock(*args, **kwargs)
        self.awaited = _AwaitEvent(self)
        self.await_count = 0
        self.await_args = None
        self.await_args_list = unittest.mock._CallList()


def create_autospec(spec, spec_set=False, instance=False, _parent=None,
                    _name=None, **kwargs):
    """
    Create a mock object using another object as a spec. Attributes on the mock
    will use the corresponding attribute on the spec object as their spec.

    ``spec`` can be a coroutine function, a class or object with coroutine
    functions as attributes.

    If ``spec`` is a coroutine function, and ``instance`` is not ``False``, a
    :exc:`RuntimeError` is raised.

    .. versionadded:: 0.12
    """
    if unittest.mock._is_list(spec):
        spec = type(spec)

    is_type = isinstance(spec, type)
    is_coroutine_func = asyncio.iscoroutinefunction(spec)

    _kwargs = {'spec': spec}
    if spec_set:
        _kwargs = {'spec_set': spec}
    elif spec is None:
        # None we mock with a normal mock without a spec
        _kwargs = {}
    if _kwargs and instance:
        _kwargs['_spec_as_instance'] = True

    _kwargs.update(kwargs)

    Klass = MagicMock
    if inspect.isdatadescriptor(spec):
        _kwargs = {}
    elif is_coroutine_func:
        if instance:
            raise RuntimeError("Instance can not be True when create_autospec "
                               "is mocking a coroutine function")
        Klass = CoroutineMock
    elif not unittest.mock._callable(spec):
        Klass = NonCallableMagicMock
    elif is_type and instance and not unittest.mock._instance_callable(spec):
        Klass = NonCallableMagicMock

    _name = _kwargs.pop('name', _name)

    _new_name = _name
    if _parent is None:
        _new_name = ''

    mock = Klass(parent=_parent, _new_parent=_parent, _new_name=_new_name,
                 name=_name, **_kwargs)

    if isinstance(spec, unittest.mock.FunctionTypes):
        wrapped_mock = mock
        # _set_signature returns an object wrapping the mock, not the mock
        # itself.
        mock = unittest.mock._set_signature(mock, spec)
        if is_coroutine_func:
            # Can't wrap the mock with asyncio.coroutine because it doesn't
            # detect a CoroWrapper as an awaitable in debug mode.
            # It is safe to do so because the mock object wrapped by
            # _set_signature returns the result of the CoroutineMock itself,
            # which is a Coroutine (as defined in CoroutineMock._mock_call)
            mock._is_coroutine = _is_coroutine
            mock.awaited = _AwaitEvent(mock)
            mock.await_count = 0
            mock.await_args = None
            mock.await_args_list = unittest.mock._CallList()

            for a in ('assert_awaited',
                      'assert_awaited_once',
                      'assert_awaited_with',
                      'assert_awaited_once_with',
                      'assert_any_await',
                      'assert_has_awaits',
                      'assert_not_awaited'):
                setattr(mock, a, getattr(wrapped_mock, a))
    else:
        unittest.mock._check_signature(spec, mock, is_type, instance)

    if _parent is not None and not instance:
        _parent._mock_children[_name] = mock

    if is_type and not instance and 'return_value' not in kwargs:
        mock.return_value = create_autospec(spec, spec_set, instance=True,
                                            _name='()', _parent=mock)

    for entry in dir(spec):
        if unittest.mock._is_magic(entry):
            continue
        try:
            original = getattr(spec, entry)
        except AttributeError:
            continue

        kwargs = {'spec': original}
        if spec_set:
            kwargs = {'spec_set': original}

        if not isinstance(original, unittest.mock.FunctionTypes):
            new = unittest.mock._SpecState(original, spec_set, mock, entry,
                                           instance)
            mock._mock_children[entry] = new
        else:
            parent = mock
            if isinstance(spec, unittest.mock.FunctionTypes):
                parent = mock.mock

            skipfirst = unittest.mock._must_skip(spec, entry, is_type)
            kwargs['_eat_self'] = skipfirst
            if asyncio.iscoroutinefunction(original):
                child_klass = CoroutineMock
            else:
                child_klass = MagicMock
            new = child_klass(parent=parent, name=entry, _new_name=entry,
                              _new_parent=parent, **kwargs)
            mock._mock_children[entry] = new
            unittest.mock._check_signature(original, new, skipfirst=skipfirst)

        if isinstance(new, unittest.mock.FunctionTypes):
            setattr(mock, entry, new)

    return mock


def mock_open(mock=None, read_data=''):
    """
    A helper function to create a mock to replace the use of :func:`open()`. It
    works for :func:`open()` called directly or used as a context manager.

    :param mock: mock object to configure, by default
                 a :class:`~asynctest.MagicMock` object is
                 created with the API limited to methods or attributes
                 available on standard file handles.

    :param read_data: string for the :func:`read()` and :func:`readlines()` of
                      the file handle to return. This is an empty string by
                      default.
    """
    if mock is None:
        mock = MagicMock(name='open', spec=open)

    return unittest.mock.mock_open(mock, read_data)


ANY = unittest.mock.ANY
DEFAULT = unittest.mock.sentinel.DEFAULT


def _update_new_callable(patcher, new, new_callable):
    if new == DEFAULT and not new_callable:
        original = patcher.get_original()[0]
        if isinstance(original, (classmethod, staticmethod)):
            # the original object is the raw descriptor, if it's a classmethod
            # or a static method, we need to unwrap it
            original = original.__get__(None, object)

        if asyncio.iscoroutinefunction(original):
            patcher.new_callable = CoroutineMock
        else:
            patcher.new_callable = MagicMock

    return patcher


# Documented in doc/asynctest.mock.rst
PatchScope = enum.Enum('PatchScope', 'LIMITED GLOBAL')
LIMITED = PatchScope.LIMITED
GLOBAL = PatchScope.GLOBAL


def _decorate_coroutine_callable(func, new_patching):
    if hasattr(func, 'patchings'):
        func.patchings.append(new_patching)
        return func

    # Python 3.5 returns True for is_generator_func(new_style_coroutine) if
    # there is an "await" statement in the function body, which is wrong. It is
    # fixed in 3.6, but I can't find which commit fixes this.
    # The only way to work correctly with 3.5 and 3.6 seems to use
    # inspect.iscoroutinefunction()
    is_generator_func = inspect.isgeneratorfunction(func)
    is_coroutine_func = asyncio.iscoroutinefunction(func)
    try:
        is_native_coroutine_func = inspect.iscoroutinefunction(func)
    except AttributeError:
        is_native_coroutine_func = False

    if not (is_generator_func or is_coroutine_func):
        return None

    patchings = [new_patching]

    def patched_factory(*args, **kwargs):
        extra_args = []
        patchers_to_exit = []
        patch_dict_with_limited_scope = []

        exc_info = tuple()
        try:
            for patching in patchings:
                arg = patching.__enter__()
                if patching.scope == LIMITED:
                    patchers_to_exit.append(patching)
                if isinstance(patching, _patch_dict):
                    if patching.scope == GLOBAL:
                        for limited_patching in patch_dict_with_limited_scope:
                            if limited_patching.in_dict is patching.in_dict:
                                limited_patching._keep_global_patch(patching)
                    else:
                        patch_dict_with_limited_scope.append(patching)
                else:
                    if patching.attribute_name is not None:
                        kwargs.update(arg)
                        if patching.new is DEFAULT:
                            patching.new = arg[patching.attribute_name]
                    elif patching.new is DEFAULT:
                        patching.mock_to_reuse = arg
                        extra_args.append(arg)

            args += tuple(extra_args)
            gen = func(*args, **kwargs)
            return _PatchedGenerator(gen, patchings,
                                     asyncio.iscoroutinefunction(func))
        except BaseException:
            if patching not in patchers_to_exit and _is_started(patching):
                # the patcher may have been started, but an exception
                # raised whilst entering one of its additional_patchers
                patchers_to_exit.append(patching)
            # Pass the exception to __exit__
            exc_info = sys.exc_info()
            # re-raise the exception
            raise
        finally:
            for patching in reversed(patchers_to_exit):
                patching.__exit__(*exc_info)

    # wrap the factory in a native coroutine or a generator to respect
    # introspection.
    if is_native_coroutine_func:
        # inspect.iscoroutinefunction() returns True
        patched = _make_native_coroutine(patched_factory)
    elif is_generator_func:
        # inspect.isgeneratorfunction() returns True
        def patched_generator(*args, **kwargs):
            return (yield from patched_factory(*args, **kwargs))

        patched = patched_generator

        if is_coroutine_func:
            # asyncio.iscoroutinefunction() returns True
            patched = asyncio.coroutine(patched)
    else:
        patched = patched_factory

    patched.patchings = patchings
    return functools.wraps(func)(patched)


class _PatchedGenerator(asyncio.coroutines.CoroWrapper):
    # Inheriting from asyncio.CoroWrapper gives us a comprehensive wrapper
    # implementing one or more workarounds for cpython bugs
    def __init__(self, gen, patchings, is_coroutine):
        self.gen = gen
        self._is_coroutine = is_coroutine
        self.__name__ = getattr(gen, '__name__', None)
        self.__qualname__ = getattr(gen, '__qualname__', None)
        self.patchings = patchings
        self.global_patchings = [p for p in patchings if p.scope == GLOBAL]
        self.limited_patchings = [p for p in patchings if p.scope == LIMITED]

        # GLOBAL patches have been started in the _patch/patched() wrapper

    def _limited_patchings_stack(self):
        with contextlib.ExitStack() as stack:
            for patching in self.limited_patchings:
                stack.enter_context(patching)

            return stack.pop_all()

    def _stop_global_patchings(self):
        for patching in reversed(self.global_patchings):
            if _is_started(patching):
                patching.stop()

    def __repr__(self):
        return repr(self.generator)

    def __next__(self):
        try:
            with self._limited_patchings_stack():
                return self.gen.send(None)
        except BaseException:
            # the generator/coroutine terminated, stop the patchings
            self._stop_global_patchings()
            raise

    def send(self, value):
        with self._limited_patchings_stack():
            return super().send(value)

    def throw(self, exc, value=None, traceback=None):
        with self._limited_patchings_stack():
            return self.gen.throw(exc, value, traceback)

    def close(self):
        try:
            with self._limited_patchings_stack():
                return self.gen.close()
        finally:
            self._stop_global_patchings()

    def __del__(self):
        # The generator/coroutine is deleted before it terminated, we must
        # still stop the patchings
        self._stop_global_patchings()


class _patch(unittest.mock._patch):
    def __init__(self, *args, scope=GLOBAL, **kwargs):
        super().__init__(*args, **kwargs)
        self.scope = scope
        self.mock_to_reuse = None

    def copy(self):
        patcher = _patch(
            self.getter, self.attribute, self.new, self.spec,
            self.create, self.spec_set,
            self.autospec, self.new_callable, self.kwargs,
            scope=self.scope)
        patcher.attribute_name = self.attribute_name
        patcher.additional_patchers = [
            p.copy() for p in self.additional_patchers
        ]
        return patcher

    def __enter__(self):
        # When patching a coroutine, we reuse the same mock object
        if self.mock_to_reuse is not None:
            self.target = self.getter()
            self.temp_original, self.is_local = self.get_original()
            setattr(self.target, self.attribute, self.mock_to_reuse)
            if self.attribute_name is not None:
                for patching in self.additional_patchers:
                    patching.__enter__()
            return self.mock_to_reuse
        else:
            return self._perform_patch()

    def _perform_patch(self):
        # This will intercept the result of super().__enter__() if we need to
        # override the default behavior (ie: we need to use our own autospec).
        original, local = self.get_original()
        result = super().__enter__()

        if self.autospec is None or not self.autospec:
            # no need to override the default behavior
            return result

        if self.autospec is True:
            autospec = original
        else:
            autospec = self.autospec

        new = create_autospec(autospec, spec_set=bool(self.spec_set),
                              _name=self.attribute, **self.kwargs)

        self.temp_original = original
        self.is_local = local
        setattr(self.target, self.attribute, new)

        if self.attribute_name is not None:
            if self.new is DEFAULT:
                result[self.attribute_name] = new
            return result

        return new

    def decorate_callable(self, func):
        wrapped = _decorate_coroutine_callable(func, self)
        if wrapped is None:
            return super().decorate_callable(func)
        else:
            return wrapped


def patch(target, new=DEFAULT, spec=None, create=False, spec_set=None,
          autospec=None, new_callable=None, scope=GLOBAL, **kwargs):
    """
    A context manager, function decorator or class decorator which patches the
    target with the value given by the ``new`` argument.

    ``new`` specifies which object will replace the ``target`` when the patch
    is applied. By default, the target will be patched with an instance of
    :class:`~asynctest.CoroutineMock` if it is a coroutine, or
    a :class:`~asynctest.MagicMock` object.

    It is a replacement to :func:`unittest.mock.patch`, but using
    :mod:`asynctest.mock` objects.

    When a generator or a coroutine is patched using the decorator, the patch
    is activated or deactivated according to the ``scope`` argument value:

      * :const:`asynctest.GLOBAL`: the default, enables the patch until the
        generator or the coroutine finishes (returns or raises an exception),

      * :const:`asynctest.LIMITED`: the patch will be activated when the
        generator or coroutine is being executed, and deactivated when it
        yields a value and pauses its execution (with ``yield``, ``yield from``
        or ``await``).

    The behavior differs from :func:`unittest.mock.patch` for generators.

    When used as a context manager, the patch is still active even if the
    generator or coroutine is paused, which may affect concurrent tasks::

        @asyncio.coroutine
        def coro():
            with asynctest.mock.patch("module.function"):
                yield from asyncio.get_event_loop().sleep(1)

        @asyncio.coroutine
        def independent_coro():
            assert not isinstance(module.function, asynctest.mock.Mock)

        asyncio.create_task(coro())
        asyncio.create_task(independent_coro())
        # this will raise an AssertionError(coro() is scheduled first)!
        loop.run_forever()

    :param scope: :const:`asynctest.GLOBAL` or :const:`asynctest.LIMITED`,
        controls when the patch is activated on generators and coroutines

    When used as a decorator with a generator based coroutine, the order of
    the decorators matters. The order of the ``@patch()`` decorators is in
    the reverse order of the parameters produced by these patches for the
    patched function. And the ``@asyncio.coroutine`` decorator should be
    the last since ``@patch()`` conceptually patches the coroutine, not
    the function::

        @patch("module.function2")
        @patch("module.function1")
        @asyncio.coroutine
        def test_coro(self, mock_function1, mock_function2):
            yield from asyncio.get_event_loop().sleep(1)

    see :func:`unittest.mock.patch()`.

    .. versionadded:: 0.6 patch into generators and coroutines with
                      a decorator.
    """
    getter, attribute = unittest.mock._get_target(target)
    patcher = _patch(getter, attribute, new, spec, create, spec_set, autospec,
                     new_callable, kwargs, scope=scope)

    return _update_new_callable(patcher, new, new_callable)


def _patch_object(target, attribute, new=DEFAULT, spec=None, create=False,
                  spec_set=None, autospec=None, new_callable=None,
                  scope=GLOBAL, **kwargs):
    patcher = _patch(lambda: target, attribute, new, spec, create, spec_set,
                     autospec, new_callable, kwargs, scope=scope)

    return _update_new_callable(patcher, new, new_callable)


def _patch_multiple(target, spec=None, create=False, spec_set=None,
                    autospec=None, new_callable=None, scope=GLOBAL, **kwargs):
    if type(target) is str:
        def getter():
            return unittest.mock._importer(target)
    else:
        def getter():
            return target

    if not kwargs:
        raise ValueError('Must supply at least one keyword argument with '
                         'patch.multiple')

    items = list(kwargs.items())
    attribute, new = items[0]
    patcher = _patch(getter, attribute, new, spec, create, spec_set, autospec,
                     new_callable, {}, scope=scope)

    patcher.attribute_name = attribute
    for attribute, new in items[1:]:
        this_patcher = _patch(getter, attribute, new, spec, create, spec_set,
                              autospec, new_callable, {}, scope=scope)
        this_patcher.attribute_name = attribute
        patcher.additional_patchers.append(this_patcher)

    def _update(patcher):
        return _update_new_callable(patcher, patcher.new, new_callable)

    patcher = _update(patcher)
    patcher.additional_patchers = list(map(_update,
                                           patcher.additional_patchers))

    return patcher


class _patch_dict(unittest.mock._patch_dict):
    # documentation is in doc/asynctest.mock.rst
    def __init__(self, in_dict, values=(), clear=False, scope=GLOBAL,
                 **kwargs):
        super().__init__(in_dict, values, clear, **kwargs)
        self.scope = scope
        self._is_started = False
        self._global_patchings = []

    def _keep_global_patch(self, other_patching):
        self._global_patchings.append(other_patching)

    def decorate_class(self, klass):
        for attr in dir(klass):
            attr_value = getattr(klass, attr)
            if (attr.startswith(patch.TEST_PREFIX) and
                    hasattr(attr_value, "__call__")):
                decorator = _patch_dict(self.in_dict, self.values, self.clear)
                decorated = decorator(attr_value)
                setattr(klass, attr, decorated)
        return klass

    def __call__(self, func):
        if isinstance(func, type):
            return self.decorate_class(func)

        wrapper = _decorate_coroutine_callable(func, self)
        if wrapper is None:
            return super().__call__(func)
        else:
            return wrapper

    def _patch_dict(self):
        self._is_started = True

        # Since Python 3.7.3, the moment when a dict specified by a target
        # string has been corrected. (see #115)
        if isinstance(self.in_dict, str):
            self.in_dict = unittest.mock._importer(self.in_dict)

        try:
            self._original = self.in_dict.copy()
        except AttributeError:
            # dict like object with no copy method
            # must support iteration over keys
            self._original = {}
            for key in self.in_dict:
                self._original[key] = self.in_dict[key]

        if self.clear:
            _clear_dict(self.in_dict)

        try:
            self.in_dict.update(self.values)
        except AttributeError:
            # dict like object with no update method
            for key in self.values:
                self.in_dict[key] = self.values[key]

    def _unpatch_dict(self):
        self._is_started = False

        if self.scope == LIMITED:
            # add to self.values the updated values which where not in
            # the original dict, as the patch may be reactivated
            for key in self.in_dict:
                if (key not in self._original or
                        self._original[key] is not self.in_dict[key]):
                    self.values[key] = self.in_dict[key]

        _clear_dict(self.in_dict)

        originals = [self._original]
        for patching in self._global_patchings:
            if patching._is_started:
                # keep the values of global patches
                originals.append(patching.values)

        for original in originals:
            try:
                self.in_dict.update(original)
            except AttributeError:
                for key in original:
                    self.in_dict[key] = original[key]


_clear_dict = unittest.mock._clear_dict

patch.object = _patch_object
patch.dict = _patch_dict
patch.multiple = _patch_multiple
patch.stopall = unittest.mock._patch_stopall
patch.TEST_PREFIX = 'test'


sentinel = unittest.mock.sentinel
call = unittest.mock.call
PropertyMock = unittest.mock.PropertyMock


def return_once(value, then=None):
    """
    Helper to use with ``side_effect``, so a mock will return a given value
    only once, then return another value.

    When used as a ``side_effect`` value, if one of ``value`` or ``then`` is an
    :class:`Exception` type, an instance of this exception will be raised.

    >>> mock.recv = Mock(side_effect=return_once(b"data"))
    >>> mock.recv()
    b"data"
    >>> repr(mock.recv())
    'None'
    >>> repr(mock.recv())
    'None'

    >>> mock.recv = Mock(side_effect=return_once(b"data", then=BlockingIOError))
    >>> mock.recv()
    b"data"
    >>> mock.recv()
    Traceback BlockingIOError

    :param value: value to be returned once by the mock when called.

    :param then: value returned for any subsequent call.

    .. versionadded:: 0.4
    """
    yield value
    while True:
        yield then
