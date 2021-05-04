import sys
import os
import abc
import contextlib
import collections
import pickle
import subprocess
import types
from unittest import TestCase, main, skipUnless, skipIf
from typing import TypeVar, Optional
from typing import T, KT, VT  # Not in __all__.
from typing import Tuple, List, Dict, Iterator
from typing import Generic
from typing import no_type_check
from typing_extensions import NoReturn, ClassVar, Final, IntVar, Literal, Type, NewType, TypedDict
from typing_extensions import TypeAlias
try:
    from typing_extensions import Protocol, runtime, runtime_checkable
except ImportError:
    pass
try:
    from typing_extensions import Annotated
except ImportError:
    pass
try:
    from typing_extensions import get_type_hints
except ImportError:
    from typing import get_type_hints

import typing
import typing_extensions
import collections.abc as collections_abc

PEP_560 = sys.version_info[:3] >= (3, 7, 0)

OLD_GENERICS = False
try:
    from typing import _type_vars, _next_in_mro, _type_check  # noqa
except ImportError:
    OLD_GENERICS = True

# We assume Python versions *below* 3.5.0 will have the most
# up-to-date version of the typing module installed. Since
# the typing module isn't a part of the standard library in older
# versions of Python, those users are likely to have a reasonably
# modern version of `typing` installed from PyPi.
TYPING_LATEST = sys.version_info[:3] < (3, 5, 0)

# Flags used to mark tests that only apply after a specific
# version of the typing module.
TYPING_3_5_1 = TYPING_LATEST or sys.version_info[:3] >= (3, 5, 1)
TYPING_3_5_3 = TYPING_LATEST or sys.version_info[:3] >= (3, 5, 3)
TYPING_3_6_1 = TYPING_LATEST or sys.version_info[:3] >= (3, 6, 1)

# For typing versions where issubclass(...) and
# isinstance(...) checks are forbidden.
#
# See https://github.com/python/typing/issues/136
# and https://github.com/python/typing/pull/283
SUBCLASS_CHECK_FORBIDDEN = TYPING_3_5_3

# For typing versions where instantiating collection
# types are allowed.
#
# See https://github.com/python/typing/issues/367
CAN_INSTANTIATE_COLLECTIONS = TYPING_3_6_1

# For Python versions supporting async/await and friends.
ASYNCIO = sys.version_info[:2] >= (3, 5)

# For checks reliant on Python 3.6 syntax changes (e.g. classvar)
PY36 = sys.version_info[:2] >= (3, 6)

# Protocols are hard to backport to the original version of typing 3.5.0
HAVE_PROTOCOLS = sys.version_info[:3] != (3, 5, 0)


class BaseTestCase(TestCase):
    def assertIsSubclass(self, cls, class_or_tuple, msg=None):
        if not issubclass(cls, class_or_tuple):
            message = '%r is not a subclass of %r' % (cls, class_or_tuple)
            if msg is not None:
                message += ' : %s' % msg
            raise self.failureException(message)

    def assertNotIsSubclass(self, cls, class_or_tuple, msg=None):
        if issubclass(cls, class_or_tuple):
            message = '%r is a subclass of %r' % (cls, class_or_tuple)
            if msg is not None:
                message += ' : %s' % msg
            raise self.failureException(message)


class Employee:
    pass


class NoReturnTests(BaseTestCase):

    def test_noreturn_instance_type_error(self):
        with self.assertRaises(TypeError):
            isinstance(42, NoReturn)

    def test_noreturn_subclass_type_error_1(self):
        with self.assertRaises(TypeError):
            issubclass(Employee, NoReturn)

    @skipUnless(SUBCLASS_CHECK_FORBIDDEN, "Behavior added in typing 3.5.3")
    def test_noreturn_subclass_type_error_2(self):
        with self.assertRaises(TypeError):
            issubclass(NoReturn, Employee)

    def test_repr(self):
        if hasattr(typing, 'NoReturn'):
            self.assertEqual(repr(NoReturn), 'typing.NoReturn')
        else:
            self.assertEqual(repr(NoReturn), 'typing_extensions.NoReturn')

    def test_not_generic(self):
        with self.assertRaises(TypeError):
            NoReturn[int]

    def test_cannot_subclass(self):
        with self.assertRaises(TypeError):
            class A(NoReturn):
                pass
        if SUBCLASS_CHECK_FORBIDDEN:
            with self.assertRaises(TypeError):
                class A(type(NoReturn)):
                    pass

    def test_cannot_instantiate(self):
        with self.assertRaises(TypeError):
            NoReturn()
        with self.assertRaises(TypeError):
            type(NoReturn)()


class ClassVarTests(BaseTestCase):

    def test_basics(self):
        with self.assertRaises(TypeError):
            ClassVar[1]
        with self.assertRaises(TypeError):
            ClassVar[int, str]
        with self.assertRaises(TypeError):
            ClassVar[int][str]

    def test_repr(self):
        if hasattr(typing, 'ClassVar'):
            mod_name = 'typing'
        else:
            mod_name = 'typing_extensions'
        self.assertEqual(repr(ClassVar), mod_name + '.ClassVar')
        cv = ClassVar[int]
        self.assertEqual(repr(cv), mod_name + '.ClassVar[int]')
        cv = ClassVar[Employee]
        self.assertEqual(repr(cv), mod_name + '.ClassVar[%s.Employee]' % __name__)

    @skipUnless(SUBCLASS_CHECK_FORBIDDEN, "Behavior added in typing 3.5.3")
    def test_cannot_subclass(self):
        with self.assertRaises(TypeError):
            class C(type(ClassVar)):
                pass
        with self.assertRaises(TypeError):
            class C(type(ClassVar[int])):
                pass

    def test_cannot_init(self):
        with self.assertRaises(TypeError):
            ClassVar()
        with self.assertRaises(TypeError):
            type(ClassVar)()
        with self.assertRaises(TypeError):
            type(ClassVar[Optional[int]])()

    def test_no_isinstance(self):
        with self.assertRaises(TypeError):
            isinstance(1, ClassVar[int])
        with self.assertRaises(TypeError):
            issubclass(int, ClassVar)


class FinalTests(BaseTestCase):

    def test_basics(self):
        with self.assertRaises(TypeError):
            Final[1]
        with self.assertRaises(TypeError):
            Final[int, str]
        with self.assertRaises(TypeError):
            Final[int][str]

    def test_repr(self):
        if hasattr(typing, 'Final') and sys.version_info[:2] >= (3, 7):
            mod_name = 'typing'
        else:
            mod_name = 'typing_extensions'
        self.assertEqual(repr(Final), mod_name + '.Final')
        cv = Final[int]
        self.assertEqual(repr(cv), mod_name + '.Final[int]')
        cv = Final[Employee]
        self.assertEqual(repr(cv), mod_name + '.Final[%s.Employee]' % __name__)

    @skipUnless(SUBCLASS_CHECK_FORBIDDEN, "Behavior added in typing 3.5.3")
    def test_cannot_subclass(self):
        with self.assertRaises(TypeError):
            class C(type(Final)):
                pass
        with self.assertRaises(TypeError):
            class C(type(Final[int])):
                pass

    def test_cannot_init(self):
        with self.assertRaises(TypeError):
            Final()
        with self.assertRaises(TypeError):
            type(Final)()
        with self.assertRaises(TypeError):
            type(Final[Optional[int]])()

    def test_no_isinstance(self):
        with self.assertRaises(TypeError):
            isinstance(1, Final[int])
        with self.assertRaises(TypeError):
            issubclass(int, Final)


class IntVarTests(BaseTestCase):
    def test_valid(self):
        T_ints = IntVar("T_ints")  # noqa

    def test_invalid(self):
        with self.assertRaises(TypeError):
            T_ints = IntVar("T_ints", int)
        with self.assertRaises(TypeError):
            T_ints = IntVar("T_ints", bound=int)
        with self.assertRaises(TypeError):
            T_ints = IntVar("T_ints", covariant=True)  # noqa


class LiteralTests(BaseTestCase):
    def test_basics(self):
        Literal[1]
        Literal[1, 2, 3]
        Literal["x", "y", "z"]
        Literal[None]

    def test_illegal_parameters_do_not_raise_runtime_errors(self):
        # Type checkers should reject these types, but we do not
        # raise errors at runtime to maintain maximum flexibility
        Literal[int]
        Literal[Literal[1, 2], Literal[4, 5]]
        Literal[3j + 2, ..., ()]
        Literal[b"foo", u"bar"]
        Literal[{"foo": 3, "bar": 4}]
        Literal[T]

    def test_literals_inside_other_types(self):
        List[Literal[1, 2, 3]]
        List[Literal[("foo", "bar", "baz")]]

    def test_repr(self):
        if hasattr(typing, 'Literal'):
            mod_name = 'typing'
        else:
            mod_name = 'typing_extensions'
        self.assertEqual(repr(Literal[1]), mod_name + ".Literal[1]")
        self.assertEqual(repr(Literal[1, True, "foo"]), mod_name + ".Literal[1, True, 'foo']")
        self.assertEqual(repr(Literal[int]), mod_name + ".Literal[int]")
        self.assertEqual(repr(Literal), mod_name + ".Literal")
        self.assertEqual(repr(Literal[None]), mod_name + ".Literal[None]")

    def test_cannot_init(self):
        with self.assertRaises(TypeError):
            Literal()
        with self.assertRaises(TypeError):
            Literal[1]()
        with self.assertRaises(TypeError):
            type(Literal)()
        with self.assertRaises(TypeError):
            type(Literal[1])()

    def test_no_isinstance_or_issubclass(self):
        with self.assertRaises(TypeError):
            isinstance(1, Literal[1])
        with self.assertRaises(TypeError):
            isinstance(int, Literal[1])
        with self.assertRaises(TypeError):
            issubclass(1, Literal[1])
        with self.assertRaises(TypeError):
            issubclass(int, Literal[1])

    def test_no_subclassing(self):
        with self.assertRaises(TypeError):
            class Foo(Literal[1]): pass
        with self.assertRaises(TypeError):
            class Bar(Literal): pass

    def test_no_multiple_subscripts(self):
        with self.assertRaises(TypeError):
            Literal[1][1]


class OverloadTests(BaseTestCase):

    def test_overload_fails(self):
        from typing_extensions import overload

        with self.assertRaises(RuntimeError):

            @overload
            def blah():
                pass

            blah()

    def test_overload_succeeds(self):
        from typing_extensions import overload

        @overload
        def blah():
            pass

        def blah():
            pass

        blah()


ASYNCIO_TESTS = """
from typing import Iterable
from typing_extensions import Awaitable, AsyncIterator

T_a = TypeVar('T_a')

class AwaitableWrapper(Awaitable[T_a]):

    def __init__(self, value):
        self.value = value

    def __await__(self) -> typing.Iterator[T_a]:
        yield
        return self.value

class AsyncIteratorWrapper(AsyncIterator[T_a]):

    def __init__(self, value: Iterable[T_a]):
        self.value = value

    def __aiter__(self) -> AsyncIterator[T_a]:
        return self

    async def __anext__(self) -> T_a:
        data = await self.value
        if data:
            return data
        else:
            raise StopAsyncIteration

class ACM:
    async def __aenter__(self) -> int:
        return 42
    async def __aexit__(self, etype, eval, tb):
        return None
"""

if ASYNCIO:
    try:
        exec(ASYNCIO_TESTS)
    except ImportError:
        ASYNCIO = False
else:
    # fake names for the sake of static analysis
    asyncio = None
    AwaitableWrapper = AsyncIteratorWrapper = ACM = object

PY36_TESTS = """
from test import ann_module, ann_module2, ann_module3
from typing_extensions import AsyncContextManager
from typing import NamedTuple

class A:
    y: float
class B(A):
    x: ClassVar[Optional['B']] = None
    y: int
    b: int
class CSub(B):
    z: ClassVar['CSub'] = B()
class G(Generic[T]):
    lst: ClassVar[List[T]] = []

class Loop:
    attr: Final['Loop']

class NoneAndForward:
    parent: 'NoneAndForward'
    meaning: None

class XRepr(NamedTuple):
    x: int
    y: int = 1
    def __str__(self):
        return f'{self.x} -> {self.y}'
    def __add__(self, other):
        return 0

@runtime
class HasCallProtocol(Protocol):
    __call__: typing.Callable


async def g_with(am: AsyncContextManager[int]):
    x: int
    async with am as x:
        return x

try:
    g_with(ACM()).send(None)
except StopIteration as e:
    assert e.args[0] == 42

Label = TypedDict('Label', [('label', str)])

class Point2D(TypedDict):
    x: int
    y: int

class Point2Dor3D(Point2D, total=False):
    z: int

class LabelPoint2D(Point2D, Label): ...

class Options(TypedDict, total=False):
    log_level: int
    log_path: str

class BaseAnimal(TypedDict):
    name: str

class Animal(BaseAnimal, total=False):
    voice: str
    tail: bool

class Cat(Animal):
    fur_color: str
"""

if PY36:
    exec(PY36_TESTS)
else:
    # fake names for the sake of static analysis
    ann_module = ann_module2 = ann_module3 = None
    A = B = CSub = G = CoolEmployee = CoolEmployeeWithDefault = object
    XMeth = XRepr = HasCallProtocol = NoneAndForward = Loop = object
    Point2D = Point2Dor3D = LabelPoint2D = Options = object
    BaseAnimal = Animal = Cat = object

gth = get_type_hints


class GetTypeHintTests(BaseTestCase):
    @skipUnless(PY36, 'Python 3.6 required')
    def test_get_type_hints_modules(self):
        ann_module_type_hints = {1: 2, 'f': Tuple[int, int], 'x': int, 'y': str}
        self.assertEqual(gth(ann_module), ann_module_type_hints)
        self.assertEqual(gth(ann_module2), {})
        self.assertEqual(gth(ann_module3), {})

    @skipUnless(PY36, 'Python 3.6 required')
    def test_get_type_hints_classes(self):
        self.assertEqual(gth(ann_module.C, ann_module.__dict__),
                         {'y': Optional[ann_module.C]})
        self.assertIsInstance(gth(ann_module.j_class), dict)
        self.assertEqual(gth(ann_module.M), {'123': 123, 'o': type})
        self.assertEqual(gth(ann_module.D),
                         {'j': str, 'k': str, 'y': Optional[ann_module.C]})
        self.assertEqual(gth(ann_module.Y), {'z': int})
        self.assertEqual(gth(ann_module.h_class),
                         {'y': Optional[ann_module.C]})
        self.assertEqual(gth(ann_module.S), {'x': str, 'y': str})
        self.assertEqual(gth(ann_module.foo), {'x': int})
        self.assertEqual(gth(NoneAndForward, globals()),
                         {'parent': NoneAndForward, 'meaning': type(None)})

    @skipUnless(PY36, 'Python 3.6 required')
    def test_respect_no_type_check(self):
        @no_type_check
        class NoTpCheck:
            class Inn:
                def __init__(self, x: 'not a type'): ...  # noqa
        self.assertTrue(NoTpCheck.__no_type_check__)
        self.assertTrue(NoTpCheck.Inn.__init__.__no_type_check__)
        self.assertEqual(gth(ann_module2.NTC.meth), {})
        class ABase(Generic[T]):
            def meth(x: int): ...
        @no_type_check
        class Der(ABase): ...
        self.assertEqual(gth(ABase.meth), {'x': int})

    @skipUnless(PY36, 'Python 3.6 required')
    def test_get_type_hints_ClassVar(self):
        self.assertEqual(gth(ann_module2.CV, ann_module2.__dict__),
                         {'var': ClassVar[ann_module2.CV]})
        self.assertEqual(gth(B, globals()),
                         {'y': int, 'x': ClassVar[Optional[B]], 'b': int})
        self.assertEqual(gth(CSub, globals()),
                         {'z': ClassVar[CSub], 'y': int, 'b': int,
                          'x': ClassVar[Optional[B]]})
        self.assertEqual(gth(G), {'lst': ClassVar[List[T]]})

    @skipUnless(PY36, 'Python 3.6 required')
    def test_final_forward_ref(self):
        self.assertEqual(gth(Loop, globals())['attr'], Final[Loop])
        self.assertNotEqual(gth(Loop, globals())['attr'], Final[int])
        self.assertNotEqual(gth(Loop, globals())['attr'], Final)


class CollectionsAbcTests(BaseTestCase):

    def test_isinstance_collections(self):
        self.assertNotIsInstance(1, collections_abc.Mapping)
        self.assertNotIsInstance(1, collections_abc.Iterable)
        self.assertNotIsInstance(1, collections_abc.Container)
        self.assertNotIsInstance(1, collections_abc.Sized)
        if SUBCLASS_CHECK_FORBIDDEN:
            with self.assertRaises(TypeError):
                isinstance(collections.deque(), typing_extensions.Deque[int])
            with self.assertRaises(TypeError):
                issubclass(collections.Counter, typing_extensions.Counter[str])

    @skipUnless(ASYNCIO, 'Python 3.5 and multithreading required')
    def test_awaitable(self):
        ns = {}
        exec(
            "async def foo() -> typing_extensions.Awaitable[int]:\n"
            "    return await AwaitableWrapper(42)\n",
            globals(), ns)
        foo = ns['foo']
        g = foo()
        self.assertIsInstance(g, typing_extensions.Awaitable)
        self.assertNotIsInstance(foo, typing_extensions.Awaitable)
        g.send(None)  # Run foo() till completion, to avoid warning.

    @skipUnless(ASYNCIO, 'Python 3.5 and multithreading required')
    def test_coroutine(self):
        ns = {}
        exec(
            "async def foo():\n"
            "    return\n",
            globals(), ns)
        foo = ns['foo']
        g = foo()
        self.assertIsInstance(g, typing_extensions.Coroutine)
        with self.assertRaises(TypeError):
            isinstance(g, typing_extensions.Coroutine[int])
        self.assertNotIsInstance(foo, typing_extensions.Coroutine)
        try:
            g.send(None)
        except StopIteration:
            pass

    @skipUnless(ASYNCIO, 'Python 3.5 and multithreading required')
    def test_async_iterable(self):
        base_it = range(10)  # type: Iterator[int]
        it = AsyncIteratorWrapper(base_it)
        self.assertIsInstance(it, typing_extensions.AsyncIterable)
        self.assertIsInstance(it, typing_extensions.AsyncIterable)
        self.assertNotIsInstance(42, typing_extensions.AsyncIterable)

    @skipUnless(ASYNCIO, 'Python 3.5 and multithreading required')
    def test_async_iterator(self):
        base_it = range(10)  # type: Iterator[int]
        it = AsyncIteratorWrapper(base_it)
        if TYPING_3_5_1:
            self.assertIsInstance(it, typing_extensions.AsyncIterator)
        self.assertNotIsInstance(42, typing_extensions.AsyncIterator)

    def test_deque(self):
        self.assertIsSubclass(collections.deque, typing_extensions.Deque)
        class MyDeque(typing_extensions.Deque[int]): ...
        self.assertIsInstance(MyDeque(), collections.deque)

    def test_counter(self):
        self.assertIsSubclass(collections.Counter, typing_extensions.Counter)

    @skipUnless(CAN_INSTANTIATE_COLLECTIONS, "Behavior added in typing 3.6.1")
    def test_defaultdict_instantiation(self):
        self.assertIs(
            type(typing_extensions.DefaultDict()),
            collections.defaultdict)
        self.assertIs(
            type(typing_extensions.DefaultDict[KT, VT]()),
            collections.defaultdict)
        self.assertIs(
            type(typing_extensions.DefaultDict[str, int]()),
            collections.defaultdict)

    def test_defaultdict_subclass(self):

        class MyDefDict(typing_extensions.DefaultDict[str, int]):
            pass

        dd = MyDefDict()
        self.assertIsInstance(dd, MyDefDict)

        self.assertIsSubclass(MyDefDict, collections.defaultdict)
        if TYPING_3_5_3:
            self.assertNotIsSubclass(collections.defaultdict, MyDefDict)

    def test_chainmap_instantiation(self):
        self.assertIs(type(typing_extensions.ChainMap()), collections.ChainMap)
        self.assertIs(type(typing_extensions.ChainMap[KT, VT]()), collections.ChainMap)
        self.assertIs(type(typing_extensions.ChainMap[str, int]()), collections.ChainMap)
        class CM(typing_extensions.ChainMap[KT, VT]): ...
        if TYPING_3_5_3:
            self.assertIs(type(CM[int, str]()), CM)

    def test_chainmap_subclass(self):

        class MyChainMap(typing_extensions.ChainMap[str, int]):
            pass

        cm = MyChainMap()
        self.assertIsInstance(cm, MyChainMap)

        self.assertIsSubclass(MyChainMap, collections.ChainMap)
        if TYPING_3_5_3:
            self.assertNotIsSubclass(collections.ChainMap, MyChainMap)

    def test_deque_instantiation(self):
        self.assertIs(type(typing_extensions.Deque()), collections.deque)
        self.assertIs(type(typing_extensions.Deque[T]()), collections.deque)
        self.assertIs(type(typing_extensions.Deque[int]()), collections.deque)
        class D(typing_extensions.Deque[T]): ...
        if TYPING_3_5_3:
            self.assertIs(type(D[int]()), D)

    def test_counter_instantiation(self):
        self.assertIs(type(typing_extensions.Counter()), collections.Counter)
        self.assertIs(type(typing_extensions.Counter[T]()), collections.Counter)
        self.assertIs(type(typing_extensions.Counter[int]()), collections.Counter)
        class C(typing_extensions.Counter[T]): ...
        if TYPING_3_5_3:
            self.assertIs(type(C[int]()), C)
            if not PEP_560:
                self.assertEqual(C.__bases__, (typing_extensions.Counter,))
            else:
                self.assertEqual(C.__bases__, (collections.Counter, typing.Generic))

    def test_counter_subclass_instantiation(self):

        class MyCounter(typing_extensions.Counter[int]):
            pass

        d = MyCounter()
        self.assertIsInstance(d, MyCounter)
        self.assertIsInstance(d, collections.Counter)
        if TYPING_3_5_1:
            self.assertIsInstance(d, typing_extensions.Counter)

    @skipUnless(PY36, 'Python 3.6 required')
    def test_async_generator(self):
        ns = {}
        exec("async def f():\n"
             "    yield 42\n", globals(), ns)
        g = ns['f']()
        self.assertIsSubclass(type(g), typing_extensions.AsyncGenerator)

    @skipUnless(PY36, 'Python 3.6 required')
    def test_no_async_generator_instantiation(self):
        with self.assertRaises(TypeError):
            typing_extensions.AsyncGenerator()
        with self.assertRaises(TypeError):
            typing_extensions.AsyncGenerator[T, T]()
        with self.assertRaises(TypeError):
            typing_extensions.AsyncGenerator[int, int]()

    @skipUnless(PY36, 'Python 3.6 required')
    def test_subclassing_async_generator(self):
        class G(typing_extensions.AsyncGenerator[int, int]):
            def asend(self, value):
                pass
            def athrow(self, typ, val=None, tb=None):
                pass

        ns = {}
        exec('async def g(): yield 0', globals(), ns)
        g = ns['g']
        self.assertIsSubclass(G, typing_extensions.AsyncGenerator)
        self.assertIsSubclass(G, typing_extensions.AsyncIterable)
        self.assertIsSubclass(G, collections_abc.AsyncGenerator)
        self.assertIsSubclass(G, collections_abc.AsyncIterable)
        self.assertNotIsSubclass(type(g), G)

        instance = G()
        self.assertIsInstance(instance, typing_extensions.AsyncGenerator)
        self.assertIsInstance(instance, typing_extensions.AsyncIterable)
        self.assertIsInstance(instance, collections_abc.AsyncGenerator)
        self.assertIsInstance(instance, collections_abc.AsyncIterable)
        self.assertNotIsInstance(type(g), G)
        self.assertNotIsInstance(g, G)


class OtherABCTests(BaseTestCase):

    def test_contextmanager(self):
        @contextlib.contextmanager
        def manager():
            yield 42

        cm = manager()
        self.assertIsInstance(cm, typing_extensions.ContextManager)
        self.assertNotIsInstance(42, typing_extensions.ContextManager)

    @skipUnless(ASYNCIO, 'Python 3.5 required')
    def test_async_contextmanager(self):
        class NotACM:
            pass
        self.assertIsInstance(ACM(), typing_extensions.AsyncContextManager)
        self.assertNotIsInstance(NotACM(), typing_extensions.AsyncContextManager)
        @contextlib.contextmanager
        def manager():
            yield 42

        cm = manager()
        self.assertNotIsInstance(cm, typing_extensions.AsyncContextManager)
        if TYPING_3_5_3:
            self.assertEqual(typing_extensions.AsyncContextManager[int].__args__, (int,))
        if TYPING_3_6_1:
            with self.assertRaises(TypeError):
                isinstance(42, typing_extensions.AsyncContextManager[int])
        with self.assertRaises(TypeError):
            typing_extensions.AsyncContextManager[int, str]


class TypeTests(BaseTestCase):

    def test_type_basic(self):

        class User: pass
        class BasicUser(User): pass
        class ProUser(User): pass

        def new_user(user_class: Type[User]) -> User:
            return user_class()

        new_user(BasicUser)

    def test_type_typevar(self):

        class User: pass
        class BasicUser(User): pass
        class ProUser(User): pass

        U = TypeVar('U', bound=User)

        def new_user(user_class: Type[U]) -> U:
            return user_class()

        new_user(BasicUser)

    @skipUnless(sys.version_info[:3] != (3, 5, 2),
                'Python 3.5.2 has a somewhat buggy Type impl')
    def test_type_optional(self):
        A = Optional[Type[BaseException]]

        def foo(a: A) -> Optional[BaseException]:
            if a is None:
                return None
            else:
                return a()

        assert isinstance(foo(KeyboardInterrupt), KeyboardInterrupt)
        assert foo(None) is None


class NewTypeTests(BaseTestCase):

    def test_basic(self):
        UserId = NewType('UserId', int)
        UserName = NewType('UserName', str)
        self.assertIsInstance(UserId(5), int)
        self.assertIsInstance(UserName('Joe'), str)
        self.assertEqual(UserId(5) + 1, 6)

    def test_errors(self):
        UserId = NewType('UserId', int)
        UserName = NewType('UserName', str)
        with self.assertRaises(TypeError):
            issubclass(UserId, int)
        with self.assertRaises(TypeError):
            class D(UserName):
                pass


PY36_PROTOCOL_TESTS = """
class Coordinate(Protocol):
    x: int
    y: int

@runtime
class Point(Coordinate, Protocol):
    label: str

class MyPoint:
    x: int
    y: int
    label: str

class XAxis(Protocol):
    x: int

class YAxis(Protocol):
    y: int

@runtime
class Position(XAxis, YAxis, Protocol):
    pass

@runtime
class Proto(Protocol):
    attr: int
    def meth(self, arg: str) -> int:
        ...

class Concrete(Proto):
    pass

class Other:
    attr: int = 1
    def meth(self, arg: str) -> int:
        if arg == 'this':
            return 1
        return 0

class NT(NamedTuple):
    x: int
    y: int
"""

if PY36:
    exec(PY36_PROTOCOL_TESTS)
else:
    # fake names for the sake of static analysis
    Coordinate = Point = MyPoint = BadPoint = NT = object
    XAxis = YAxis = Position = Proto = Concrete = Other = object


if HAVE_PROTOCOLS:
    class ProtocolTests(BaseTestCase):

        def test_basic_protocol(self):
            @runtime
            class P(Protocol):
                def meth(self):
                    pass
            class C: pass
            class D:
                def meth(self):
                    pass
            def f():
                pass
            self.assertIsSubclass(D, P)
            self.assertIsInstance(D(), P)
            self.assertNotIsSubclass(C, P)
            self.assertNotIsInstance(C(), P)
            self.assertNotIsSubclass(types.FunctionType, P)
            self.assertNotIsInstance(f, P)

        def test_everything_implements_empty_protocol(self):
            @runtime
            class Empty(Protocol): pass
            class C: pass
            def f():
                pass
            for thing in (object, type, tuple, C, types.FunctionType):
                self.assertIsSubclass(thing, Empty)
            for thing in (object(), 1, (), typing, f):
                self.assertIsInstance(thing, Empty)

        @skipUnless(PY36, 'Python 3.6 required')
        def test_function_implements_protocol(self):
            def f():
                pass
            self.assertIsInstance(f, HasCallProtocol)

        def test_no_inheritance_from_nominal(self):
            class C: pass
            class BP(Protocol): pass
            with self.assertRaises(TypeError):
                class P(C, Protocol):
                    pass
            with self.assertRaises(TypeError):
                class P(Protocol, C):
                    pass
            with self.assertRaises(TypeError):
                class P(BP, C, Protocol):
                    pass
            class D(BP, C): pass
            class E(C, BP): pass
            self.assertNotIsInstance(D(), E)
            self.assertNotIsInstance(E(), D)

        def test_no_instantiation(self):
            class P(Protocol): pass
            with self.assertRaises(TypeError):
                P()
            class C(P): pass
            self.assertIsInstance(C(), C)
            T = TypeVar('T')
            class PG(Protocol[T]): pass
            with self.assertRaises(TypeError):
                PG()
            with self.assertRaises(TypeError):
                PG[int]()
            with self.assertRaises(TypeError):
                PG[T]()
            class CG(PG[T]): pass
            self.assertIsInstance(CG[int](), CG)

        def test_cannot_instantiate_abstract(self):
            @runtime
            class P(Protocol):
                @abc.abstractmethod
                def ameth(self) -> int:
                    raise NotImplementedError
            class B(P):
                pass
            class C(B):
                def ameth(self) -> int:
                    return 26
            with self.assertRaises(TypeError):
                B()
            self.assertIsInstance(C(), P)

        def test_subprotocols_extending(self):
            class P1(Protocol):
                def meth1(self):
                    pass
            @runtime
            class P2(P1, Protocol):
                def meth2(self):
                    pass
            class C:
                def meth1(self):
                    pass
                def meth2(self):
                    pass
            class C1:
                def meth1(self):
                    pass
            class C2:
                def meth2(self):
                    pass
            self.assertNotIsInstance(C1(), P2)
            self.assertNotIsInstance(C2(), P2)
            self.assertNotIsSubclass(C1, P2)
            self.assertNotIsSubclass(C2, P2)
            self.assertIsInstance(C(), P2)
            self.assertIsSubclass(C, P2)

        def test_subprotocols_merging(self):
            class P1(Protocol):
                def meth1(self):
                    pass
            class P2(Protocol):
                def meth2(self):
                    pass
            @runtime
            class P(P1, P2, Protocol):
                pass
            class C:
                def meth1(self):
                    pass
                def meth2(self):
                    pass
            class C1:
                def meth1(self):
                    pass
            class C2:
                def meth2(self):
                    pass
            self.assertNotIsInstance(C1(), P)
            self.assertNotIsInstance(C2(), P)
            self.assertNotIsSubclass(C1, P)
            self.assertNotIsSubclass(C2, P)
            self.assertIsInstance(C(), P)
            self.assertIsSubclass(C, P)

        def test_protocols_issubclass(self):
            T = TypeVar('T')
            @runtime
            class P(Protocol):
                def x(self): ...
            @runtime
            class PG(Protocol[T]):
                def x(self): ...
            class BadP(Protocol):
                def x(self): ...
            class BadPG(Protocol[T]):
                def x(self): ...
            class C:
                def x(self): ...
            self.assertIsSubclass(C, P)
            self.assertIsSubclass(C, PG)
            self.assertIsSubclass(BadP, PG)
            if not PEP_560:
                self.assertIsSubclass(PG[int], PG)
                self.assertIsSubclass(BadPG[int], P)
                self.assertIsSubclass(BadPG[T], PG)
            with self.assertRaises(TypeError):
                issubclass(C, PG[T])
            with self.assertRaises(TypeError):
                issubclass(C, PG[C])
            with self.assertRaises(TypeError):
                issubclass(C, BadP)
            with self.assertRaises(TypeError):
                issubclass(C, BadPG)
            with self.assertRaises(TypeError):
                issubclass(P, PG[T])
            with self.assertRaises(TypeError):
                issubclass(PG, PG[int])

        def test_protocols_issubclass_non_callable(self):
            class C:
                x = 1
            @runtime
            class PNonCall(Protocol):
                x = 1
            with self.assertRaises(TypeError):
                issubclass(C, PNonCall)
            self.assertIsInstance(C(), PNonCall)
            PNonCall.register(C)
            with self.assertRaises(TypeError):
                issubclass(C, PNonCall)
            self.assertIsInstance(C(), PNonCall)
            # check that non-protocol subclasses are not affected
            class D(PNonCall): ...
            self.assertNotIsSubclass(C, D)
            self.assertNotIsInstance(C(), D)
            D.register(C)
            self.assertIsSubclass(C, D)
            self.assertIsInstance(C(), D)
            with self.assertRaises(TypeError):
                issubclass(D, PNonCall)

        def test_protocols_isinstance(self):
            T = TypeVar('T')
            @runtime
            class P(Protocol):
                def meth(x): ...
            @runtime
            class PG(Protocol[T]):
                def meth(x): ...
            class BadP(Protocol):
                def meth(x): ...
            class BadPG(Protocol[T]):
                def meth(x): ...
            class C:
                def meth(x): ...
            self.assertIsInstance(C(), P)
            self.assertIsInstance(C(), PG)
            with self.assertRaises(TypeError):
                isinstance(C(), PG[T])
            with self.assertRaises(TypeError):
                isinstance(C(), PG[C])
            with self.assertRaises(TypeError):
                isinstance(C(), BadP)
            with self.assertRaises(TypeError):
                isinstance(C(), BadPG)

        @skipUnless(PY36, 'Python 3.6 required')
        def test_protocols_isinstance_py36(self):
            class APoint:
                def __init__(self, x, y, label):
                    self.x = x
                    self.y = y
                    self.label = label
            class BPoint:
                label = 'B'
                def __init__(self, x, y):
                    self.x = x
                    self.y = y
            class C:
                def __init__(self, attr):
                    self.attr = attr
                def meth(self, arg):
                    return 0
            class Bad: pass
            self.assertIsInstance(APoint(1, 2, 'A'), Point)
            self.assertIsInstance(BPoint(1, 2), Point)
            self.assertNotIsInstance(MyPoint(), Point)
            self.assertIsInstance(BPoint(1, 2), Position)
            self.assertIsInstance(Other(), Proto)
            self.assertIsInstance(Concrete(), Proto)
            self.assertIsInstance(C(42), Proto)
            self.assertNotIsInstance(Bad(), Proto)
            self.assertNotIsInstance(Bad(), Point)
            self.assertNotIsInstance(Bad(), Position)
            self.assertNotIsInstance(Bad(), Concrete)
            self.assertNotIsInstance(Other(), Concrete)
            self.assertIsInstance(NT(1, 2), Position)

        def test_protocols_isinstance_init(self):
            T = TypeVar('T')
            @runtime
            class P(Protocol):
                x = 1
            @runtime
            class PG(Protocol[T]):
                x = 1
            class C:
                def __init__(self, x):
                    self.x = x
            self.assertIsInstance(C(1), P)
            self.assertIsInstance(C(1), PG)

        def test_protocols_support_register(self):
            @runtime
            class P(Protocol):
                x = 1
            class PM(Protocol):
                def meth(self): pass
            class D(PM): pass
            class C: pass
            D.register(C)
            P.register(C)
            self.assertIsInstance(C(), P)
            self.assertIsInstance(C(), D)

        def test_none_on_non_callable_doesnt_block_implementation(self):
            @runtime
            class P(Protocol):
                x = 1
            class A:
                x = 1
            class B(A):
                x = None
            class C:
                def __init__(self):
                    self.x = None
            self.assertIsInstance(B(), P)
            self.assertIsInstance(C(), P)

        def test_none_on_callable_blocks_implementation(self):
            @runtime
            class P(Protocol):
                def x(self): ...
            class A:
                def x(self): ...
            class B(A):
                x = None
            class C:
                def __init__(self):
                    self.x = None
            self.assertNotIsInstance(B(), P)
            self.assertNotIsInstance(C(), P)

        def test_non_protocol_subclasses(self):
            class P(Protocol):
                x = 1
            @runtime
            class PR(Protocol):
                def meth(self): pass
            class NonP(P):
                x = 1
            class NonPR(PR): pass
            class C:
                x = 1
            class D:
                def meth(self): pass
            self.assertNotIsInstance(C(), NonP)
            self.assertNotIsInstance(D(), NonPR)
            self.assertNotIsSubclass(C, NonP)
            self.assertNotIsSubclass(D, NonPR)
            self.assertIsInstance(NonPR(), PR)
            self.assertIsSubclass(NonPR, PR)

        def test_custom_subclasshook(self):
            class P(Protocol):
                x = 1
            class OKClass: pass
            class BadClass:
                x = 1
            class C(P):
                @classmethod
                def __subclasshook__(cls, other):
                    return other.__name__.startswith("OK")
            self.assertIsInstance(OKClass(), C)
            self.assertNotIsInstance(BadClass(), C)
            self.assertIsSubclass(OKClass, C)
            self.assertNotIsSubclass(BadClass, C)

        def test_issubclass_fails_correctly(self):
            @runtime
            class P(Protocol):
                x = 1
            class C: pass
            with self.assertRaises(TypeError):
                issubclass(C(), P)

        @skipUnless(not OLD_GENERICS, "New style generics required")
        def test_defining_generic_protocols(self):
            T = TypeVar('T')
            S = TypeVar('S')
            @runtime
            class PR(Protocol[T, S]):
                def meth(self): pass
            class P(PR[int, T], Protocol[T]):
                y = 1
            self.assertIsSubclass(PR[int, T], PR)
            self.assertIsSubclass(P[str], PR)
            with self.assertRaises(TypeError):
                PR[int]
            with self.assertRaises(TypeError):
                P[int, str]
            with self.assertRaises(TypeError):
                PR[int, 1]
            if TYPING_3_5_3:
                with self.assertRaises(TypeError):
                    PR[int, ClassVar]
            class C(PR[int, T]): pass
            self.assertIsInstance(C[str](), C)

        def test_defining_generic_protocols_old_style(self):
            T = TypeVar('T')
            S = TypeVar('S')
            @runtime
            class PR(Protocol, Generic[T, S]):
                def meth(self): pass
            class P(PR[int, str], Protocol):
                y = 1
            if not PEP_560:
                self.assertIsSubclass(PR[int, str], PR)
            else:
                with self.assertRaises(TypeError):
                    self.assertIsSubclass(PR[int, str], PR)
            self.assertIsSubclass(P, PR)
            with self.assertRaises(TypeError):
                PR[int]
            with self.assertRaises(TypeError):
                PR[int, 1]
            class P1(Protocol, Generic[T]):
                def bar(self, x: T) -> str: ...
            class P2(Generic[T], Protocol):
                def bar(self, x: T) -> str: ...
            @runtime
            class PSub(P1[str], Protocol):
                x = 1
            class Test:
                x = 1
                def bar(self, x: str) -> str:
                    return x
            self.assertIsInstance(Test(), PSub)
            if TYPING_3_5_3:
                with self.assertRaises(TypeError):
                    PR[int, ClassVar]

        def test_init_called(self):
            T = TypeVar('T')
            class P(Protocol[T]): pass
            class C(P[T]):
                def __init__(self):
                    self.test = 'OK'
            self.assertEqual(C[int]().test, 'OK')

        @skipUnless(not OLD_GENERICS, "New style generics required")
        def test_protocols_bad_subscripts(self):
            T = TypeVar('T')
            S = TypeVar('S')
            with self.assertRaises(TypeError):
                class P(Protocol[T, T]): pass
            with self.assertRaises(TypeError):
                class P(Protocol[int]): pass
            with self.assertRaises(TypeError):
                class P(Protocol[T], Protocol[S]): pass
            with self.assertRaises(TypeError):
                class P(typing.Mapping[T, S], Protocol[T]): pass

        @skipUnless(TYPING_3_5_3, 'New style __repr__ and __eq__ only')
        def test_generic_protocols_repr(self):
            T = TypeVar('T')
            S = TypeVar('S')
            class P(Protocol[T, S]): pass
            # After PEP 560 unsubscripted generics have a standard repr.
            if not PEP_560:
                self.assertTrue(repr(P).endswith('P'))
            self.assertTrue(repr(P[T, S]).endswith('P[~T, ~S]'))
            self.assertTrue(repr(P[int, str]).endswith('P[int, str]'))

        @skipUnless(TYPING_3_5_3, 'New style __repr__ and __eq__ only')
        def test_generic_protocols_eq(self):
            T = TypeVar('T')
            S = TypeVar('S')
            class P(Protocol[T, S]): pass
            self.assertEqual(P, P)
            self.assertEqual(P[int, T], P[int, T])
            self.assertEqual(P[T, T][Tuple[T, S]][int, str],
                             P[Tuple[int, str], Tuple[int, str]])

        @skipUnless(not OLD_GENERICS, "New style generics required")
        def test_generic_protocols_special_from_generic(self):
            T = TypeVar('T')
            class P(Protocol[T]): pass
            self.assertEqual(P.__parameters__, (T,))
            self.assertIs(P.__args__, None)
            self.assertIs(P.__origin__, None)
            self.assertEqual(P[int].__parameters__, ())
            self.assertEqual(P[int].__args__, (int,))
            self.assertIs(P[int].__origin__, P)

        def test_generic_protocols_special_from_protocol(self):
            @runtime
            class PR(Protocol):
                x = 1
            class P(Protocol):
                def meth(self):
                    pass
            T = TypeVar('T')
            class PG(Protocol[T]):
                x = 1
                def meth(self):
                    pass
            self.assertTrue(P._is_protocol)
            self.assertTrue(PR._is_protocol)
            self.assertTrue(PG._is_protocol)
            if hasattr(typing, 'Protocol'):
                self.assertFalse(P._is_runtime_protocol)
            else:
                with self.assertRaises(AttributeError):
                    self.assertFalse(P._is_runtime_protocol)
            self.assertTrue(PR._is_runtime_protocol)
            self.assertTrue(PG[int]._is_protocol)
            self.assertEqual(typing_extensions._get_protocol_attrs(P), {'meth'})
            self.assertEqual(typing_extensions._get_protocol_attrs(PR), {'x'})
            self.assertEqual(frozenset(typing_extensions._get_protocol_attrs(PG)),
                             frozenset({'x', 'meth'}))
            if not PEP_560:
                self.assertEqual(frozenset(typing_extensions._get_protocol_attrs(PG[int])),
                                 frozenset({'x', 'meth'}))

        def test_no_runtime_deco_on_nominal(self):
            with self.assertRaises(TypeError):
                @runtime
                class C: pass
            class Proto(Protocol):
                x = 1
            with self.assertRaises(TypeError):
                @runtime
                class Concrete(Proto):
                    pass

        def test_none_treated_correctly(self):
            @runtime
            class P(Protocol):
                x = None  # type: int
            class B(object): pass
            self.assertNotIsInstance(B(), P)
            class C:
                x = 1
            class D:
                x = None
            self.assertIsInstance(C(), P)
            self.assertIsInstance(D(), P)
            class CI:
                def __init__(self):
                    self.x = 1
            class DI:
                def __init__(self):
                    self.x = None
            self.assertIsInstance(C(), P)
            self.assertIsInstance(D(), P)

        def test_protocols_in_unions(self):
            class P(Protocol):
                x = None  # type: int
            Alias = typing.Union[typing.Iterable, P]
            Alias2 = typing.Union[P, typing.Iterable]
            self.assertEqual(Alias, Alias2)

        def test_protocols_pickleable(self):
            global P, CP  # pickle wants to reference the class by name
            T = TypeVar('T')

            @runtime
            class P(Protocol[T]):
                x = 1
            class CP(P[int]):
                pass

            c = CP()
            c.foo = 42
            c.bar = 'abc'
            for proto in range(pickle.HIGHEST_PROTOCOL + 1):
                z = pickle.dumps(c, proto)
                x = pickle.loads(z)
                self.assertEqual(x.foo, 42)
                self.assertEqual(x.bar, 'abc')
                self.assertEqual(x.x, 1)
                self.assertEqual(x.__dict__, {'foo': 42, 'bar': 'abc'})
                s = pickle.dumps(P)
                D = pickle.loads(s)
                class E:
                    x = 1
                self.assertIsInstance(E(), D)

        def test_collections_protocols_allowed(self):
            @runtime_checkable
            class Custom(collections.abc.Iterable, Protocol):
                def close(self): pass

            class A: ...
            class B:
                def __iter__(self):
                    return []
                def close(self):
                    return 0

            self.assertIsSubclass(B, Custom)
            self.assertNotIsSubclass(A, Custom)


class TypedDictTests(BaseTestCase):

    def test_basics_iterable_syntax(self):
        Emp = TypedDict('Emp', {'name': str, 'id': int})
        self.assertIsSubclass(Emp, dict)
        self.assertIsSubclass(Emp, typing.MutableMapping)
        if sys.version_info[0] >= 3:
            import collections.abc
            self.assertNotIsSubclass(Emp, collections.abc.Sequence)
        jim = Emp(name='Jim', id=1)
        self.assertIs(type(jim), dict)
        self.assertEqual(jim['name'], 'Jim')
        self.assertEqual(jim['id'], 1)
        self.assertEqual(Emp.__name__, 'Emp')
        self.assertEqual(Emp.__module__, __name__)
        self.assertEqual(Emp.__bases__, (dict,))
        self.assertEqual(Emp.__annotations__, {'name': str, 'id': int})
        self.assertEqual(Emp.__total__, True)

    def test_basics_keywords_syntax(self):
        Emp = TypedDict('Emp', name=str, id=int)
        self.assertIsSubclass(Emp, dict)
        self.assertIsSubclass(Emp, typing.MutableMapping)
        if sys.version_info[0] >= 3:
            import collections.abc
            self.assertNotIsSubclass(Emp, collections.abc.Sequence)
        jim = Emp(name='Jim', id=1)
        self.assertIs(type(jim), dict)
        self.assertEqual(jim['name'], 'Jim')
        self.assertEqual(jim['id'], 1)
        self.assertEqual(Emp.__name__, 'Emp')
        self.assertEqual(Emp.__module__, __name__)
        self.assertEqual(Emp.__bases__, (dict,))
        self.assertEqual(Emp.__annotations__, {'name': str, 'id': int})
        self.assertEqual(Emp.__total__, True)

    def test_typeddict_special_keyword_names(self):
        TD = TypedDict("TD", cls=type, self=object, typename=str, _typename=int,
                       fields=list, _fields=dict)
        self.assertEqual(TD.__name__, 'TD')
        self.assertEqual(TD.__annotations__, {'cls': type, 'self': object, 'typename': str,
                                              '_typename': int, 'fields': list, '_fields': dict})
        a = TD(cls=str, self=42, typename='foo', _typename=53,
               fields=[('bar', tuple)], _fields={'baz', set})
        self.assertEqual(a['cls'], str)
        self.assertEqual(a['self'], 42)
        self.assertEqual(a['typename'], 'foo')
        self.assertEqual(a['_typename'], 53)
        self.assertEqual(a['fields'], [('bar', tuple)])
        self.assertEqual(a['_fields'], {'baz', set})

    @skipIf(hasattr(typing, 'TypedDict'), "Should be tested by upstream")
    def test_typeddict_create_errors(self):
        with self.assertRaises(TypeError):
            TypedDict.__new__()
        with self.assertRaises(TypeError):
            TypedDict()
        with self.assertRaises(TypeError):
            TypedDict('Emp', [('name', str)], None)

        with self.assertWarns(DeprecationWarning):
            Emp = TypedDict(_typename='Emp', name=str, id=int)
        self.assertEqual(Emp.__name__, 'Emp')
        self.assertEqual(Emp.__annotations__, {'name': str, 'id': int})

        with self.assertWarns(DeprecationWarning):
            Emp = TypedDict('Emp', _fields={'name': str, 'id': int})
        self.assertEqual(Emp.__name__, 'Emp')
        self.assertEqual(Emp.__annotations__, {'name': str, 'id': int})

    def test_typeddict_errors(self):
        Emp = TypedDict('Emp', {'name': str, 'id': int})
        if sys.version_info[:2] >= (3, 9):
            self.assertEqual(TypedDict.__module__, 'typing')
        else:
            self.assertEqual(TypedDict.__module__, 'typing_extensions')
        jim = Emp(name='Jim', id=1)
        with self.assertRaises(TypeError):
            isinstance({}, Emp)
        with self.assertRaises(TypeError):
            isinstance(jim, Emp)
        with self.assertRaises(TypeError):
            issubclass(dict, Emp)
        with self.assertRaises(TypeError):
            TypedDict('Hi', x=1)
        with self.assertRaises(TypeError):
            TypedDict('Hi', [('x', int), ('y', 1)])
        with self.assertRaises(TypeError):
            TypedDict('Hi', [('x', int)], y=int)

    @skipUnless(PY36, 'Python 3.6 required')
    def test_py36_class_syntax_usage(self):
        self.assertEqual(LabelPoint2D.__name__, 'LabelPoint2D')
        self.assertEqual(LabelPoint2D.__module__, __name__)
        self.assertEqual(LabelPoint2D.__annotations__, {'x': int, 'y': int, 'label': str})
        self.assertEqual(LabelPoint2D.__bases__, (dict,))
        self.assertEqual(LabelPoint2D.__total__, True)
        self.assertNotIsSubclass(LabelPoint2D, typing.Sequence)
        not_origin = Point2D(x=0, y=1)
        self.assertEqual(not_origin['x'], 0)
        self.assertEqual(not_origin['y'], 1)
        other = LabelPoint2D(x=0, y=1, label='hi')
        self.assertEqual(other['label'], 'hi')

    def test_pickle(self):
        global EmpD  # pickle wants to reference the class by name
        EmpD = TypedDict('EmpD', name=str, id=int)
        jane = EmpD({'name': 'jane', 'id': 37})
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            z = pickle.dumps(jane, proto)
            jane2 = pickle.loads(z)
            self.assertEqual(jane2, jane)
            self.assertEqual(jane2, {'name': 'jane', 'id': 37})
            ZZ = pickle.dumps(EmpD, proto)
            EmpDnew = pickle.loads(ZZ)
            self.assertEqual(EmpDnew({'name': 'jane', 'id': 37}), jane)

    def test_optional(self):
        EmpD = TypedDict('EmpD', name=str, id=int)

        self.assertEqual(typing.Optional[EmpD], typing.Union[None, EmpD])
        self.assertNotEqual(typing.List[EmpD], typing.Tuple[EmpD])

    def test_total(self):
        D = TypedDict('D', {'x': int}, total=False)
        self.assertEqual(D(), {})
        self.assertEqual(D(x=1), {'x': 1})
        self.assertEqual(D.__total__, False)

        if PY36:
            self.assertEqual(Options(), {})
            self.assertEqual(Options(log_level=2), {'log_level': 2})
            self.assertEqual(Options.__total__, False)

    @skipUnless(PY36, 'Python 3.6 required')
    def test_optional_keys(self):
        assert Point2Dor3D.__required_keys__ == frozenset(['x', 'y'])
        assert Point2Dor3D.__optional_keys__ == frozenset(['z'])

    @skipUnless(PY36, 'Python 3.6 required')
    def test_keys_inheritance(self):
        assert BaseAnimal.__required_keys__ == frozenset(['name'])
        assert BaseAnimal.__optional_keys__ == frozenset([])
        assert BaseAnimal.__annotations__ == {'name': str}

        assert Animal.__required_keys__ == frozenset(['name'])
        assert Animal.__optional_keys__ == frozenset(['tail', 'voice'])
        assert Animal.__annotations__ == {
            'name': str,
            'tail': bool,
            'voice': str,
        }

        assert Cat.__required_keys__ == frozenset(['name', 'fur_color'])
        assert Cat.__optional_keys__ == frozenset(['tail', 'voice'])
        assert Cat.__annotations__ == {
            'fur_color': str,
            'name': str,
            'tail': bool,
            'voice': str,
        }


@skipUnless(TYPING_3_5_3, "Python >= 3.5.3 required")
class AnnotatedTests(BaseTestCase):

    def test_repr(self):
        if hasattr(typing, 'Annotated'):
            mod_name = 'typing'
        else:
            mod_name = "typing_extensions"
        self.assertEqual(
            repr(Annotated[int, 4, 5]),
            mod_name + ".Annotated[int, 4, 5]"
        )
        self.assertEqual(
            repr(Annotated[List[int], 4, 5]),
            mod_name + ".Annotated[typing.List[int], 4, 5]"
        )

    def test_flatten(self):
        A = Annotated[Annotated[int, 4], 5]
        self.assertEqual(A, Annotated[int, 4, 5])
        self.assertEqual(A.__metadata__, (4, 5))
        if PEP_560:
            self.assertEqual(A.__origin__, int)

    def test_specialize(self):
        L = Annotated[List[T], "my decoration"]
        LI = Annotated[List[int], "my decoration"]
        self.assertEqual(L[int], Annotated[List[int], "my decoration"])
        self.assertEqual(L[int].__metadata__, ("my decoration",))
        if PEP_560:
            self.assertEqual(L[int].__origin__, List[int])
        with self.assertRaises(TypeError):
            LI[int]
        with self.assertRaises(TypeError):
            L[int, float]

    def test_hash_eq(self):
        self.assertEqual(len({Annotated[int, 4, 5], Annotated[int, 4, 5]}), 1)
        self.assertNotEqual(Annotated[int, 4, 5], Annotated[int, 5, 4])
        self.assertNotEqual(Annotated[int, 4, 5], Annotated[str, 4, 5])
        self.assertNotEqual(Annotated[int, 4], Annotated[int, 4, 4])
        self.assertEqual(
            {Annotated[int, 4, 5], Annotated[int, 4, 5], Annotated[T, 4, 5]},
            {Annotated[int, 4, 5], Annotated[T, 4, 5]}
        )

    def test_instantiate(self):
        class C:
            classvar = 4

            def __init__(self, x):
                self.x = x

            def __eq__(self, other):
                if not isinstance(other, C):
                    return NotImplemented
                return other.x == self.x

        A = Annotated[C, "a decoration"]
        a = A(5)
        c = C(5)
        self.assertEqual(a, c)
        self.assertEqual(a.x, c.x)
        self.assertEqual(a.classvar, c.classvar)

    def test_instantiate_generic(self):
        MyCount = Annotated[typing_extensions.Counter[T], "my decoration"]
        self.assertEqual(MyCount([4, 4, 5]), {4: 2, 5: 1})
        self.assertEqual(MyCount[int]([4, 4, 5]), {4: 2, 5: 1})

    def test_cannot_instantiate_forward(self):
        A = Annotated["int", (5, 6)]
        with self.assertRaises(TypeError):
            A(5)

    def test_cannot_instantiate_type_var(self):
        A = Annotated[T, (5, 6)]
        with self.assertRaises(TypeError):
            A(5)

    def test_cannot_getattr_typevar(self):
        with self.assertRaises(AttributeError):
            Annotated[T, (5, 7)].x

    def test_attr_passthrough(self):
        class C:
            classvar = 4

        A = Annotated[C, "a decoration"]
        self.assertEqual(A.classvar, 4)
        A.x = 5
        self.assertEqual(C.x, 5)

    def test_hash_eq(self):
        self.assertEqual(len({Annotated[int, 4, 5], Annotated[int, 4, 5]}), 1)
        self.assertNotEqual(Annotated[int, 4, 5], Annotated[int, 5, 4])
        self.assertNotEqual(Annotated[int, 4, 5], Annotated[str, 4, 5])
        self.assertNotEqual(Annotated[int, 4], Annotated[int, 4, 4])
        self.assertEqual(
            {Annotated[int, 4, 5], Annotated[int, 4, 5], Annotated[T, 4, 5]},
            {Annotated[int, 4, 5], Annotated[T, 4, 5]}
        )

    def test_cannot_subclass(self):
        with self.assertRaisesRegex(TypeError, "Cannot subclass .*Annotated"):
            class C(Annotated):
                pass

    def test_cannot_check_instance(self):
        with self.assertRaises(TypeError):
            isinstance(5, Annotated[int, "positive"])

    def test_cannot_check_subclass(self):
        with self.assertRaises(TypeError):
            issubclass(int, Annotated[int, "positive"])

    @skipUnless(PEP_560, "pickle support was added with PEP 560")
    def test_pickle(self):
        samples = [typing.Any, typing.Union[int, str],
                   typing.Optional[str], Tuple[int, ...],
                   typing.Callable[[str], bytes]]

        for t in samples:
            x = Annotated[t, "a"]

            for prot in range(pickle.HIGHEST_PROTOCOL + 1):
                with self.subTest(protocol=prot, type=t):
                    pickled = pickle.dumps(x, prot)
                    restored = pickle.loads(pickled)
                    self.assertEqual(x, restored)

        global _Annotated_test_G

        class _Annotated_test_G(Generic[T]):
            x = 1

        G = Annotated[_Annotated_test_G[int], "A decoration"]
        G.foo = 42
        G.bar = 'abc'

        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            z = pickle.dumps(G, proto)
            x = pickle.loads(z)
            self.assertEqual(x.foo, 42)
            self.assertEqual(x.bar, 'abc')
            self.assertEqual(x.x, 1)

    def test_subst(self):
        dec = "a decoration"
        dec2 = "another decoration"

        S = Annotated[T, dec2]
        self.assertEqual(S[int], Annotated[int, dec2])

        self.assertEqual(S[Annotated[int, dec]], Annotated[int, dec, dec2])
        L = Annotated[List[T], dec]

        self.assertEqual(L[int], Annotated[List[int], dec])
        with self.assertRaises(TypeError):
            L[int, int]

        self.assertEqual(S[L[int]], Annotated[List[int], dec, dec2])

        D = Annotated[Dict[KT, VT], dec]
        self.assertEqual(D[str, int], Annotated[Dict[str, int], dec])
        with self.assertRaises(TypeError):
            D[int]

        It = Annotated[int, dec]
        with self.assertRaises(TypeError):
            It[None]

        LI = L[int]
        with self.assertRaises(TypeError):
            LI[None]

    def test_annotated_in_other_types(self):
        X = List[Annotated[T, 5]]
        self.assertEqual(X[int], List[Annotated[int, 5]])


@skipUnless(PEP_560, "Python 3.7 required")
class GetTypeHintsTests(BaseTestCase):
    def test_get_type_hints(self):
        def foobar(x: List['X']): ...
        X = Annotated[int, (1, 10)]
        self.assertEqual(
            get_type_hints(foobar, globals(), locals()),
            {'x': List[int]}
        )
        self.assertEqual(
            get_type_hints(foobar, globals(), locals(), include_extras=True),
            {'x': List[Annotated[int, (1, 10)]]}
        )
        BA = Tuple[Annotated[T, (1, 0)], ...]
        def barfoo(x: BA): ...
        self.assertEqual(get_type_hints(barfoo, globals(), locals())['x'], Tuple[T, ...])
        self.assertIs(
            get_type_hints(barfoo, globals(), locals(), include_extras=True)['x'],
            BA
        )
        def barfoo2(x: typing.Callable[..., Annotated[List[T], "const"]],
                    y: typing.Union[int, Annotated[T, "mutable"]]): ...
        self.assertEqual(
            get_type_hints(barfoo2, globals(), locals()),
            {'x': typing.Callable[..., List[T]], 'y': typing.Union[int, T]}
        )
        BA2 = typing.Callable[..., List[T]]
        def barfoo3(x: BA2): ...
        self.assertIs(
            get_type_hints(barfoo3, globals(), locals(), include_extras=True)["x"],
            BA2
        )

    def test_get_type_hints_refs(self):

        Const = Annotated[T, "Const"]

        class MySet(Generic[T]):

            def __ior__(self, other: "Const[MySet[T]]") -> "MySet[T]":
                ...

            def __iand__(self, other: Const["MySet[T]"]) -> "MySet[T]":
                ...

        self.assertEqual(
            get_type_hints(MySet.__iand__, globals(), locals()),
            {'other': MySet[T], 'return': MySet[T]}
        )

        self.assertEqual(
            get_type_hints(MySet.__iand__, globals(), locals(), include_extras=True),
            {'other': Const[MySet[T]], 'return': MySet[T]}
        )

        self.assertEqual(
            get_type_hints(MySet.__ior__, globals(), locals()),
            {'other': MySet[T], 'return': MySet[T]}
        )


class TypeAliasTests(BaseTestCase):
    @skipUnless(PY36, 'Python 3.6 required')
    def test_canonical_usage_with_variable_annotation(self):
        ns = {}
        exec('Alias: TypeAlias = Employee', globals(), ns)

    def test_canonical_usage_with_type_comment(self):
        Alias = Employee  # type: TypeAlias

    def test_cannot_instantiate(self):
        with self.assertRaises(TypeError):
            TypeAlias()

    def test_no_isinstance(self):
        with self.assertRaises(TypeError):
            isinstance(42, TypeAlias)

    def test_no_issubclass(self):
        with self.assertRaises(TypeError):
            issubclass(Employee, TypeAlias)

        if SUBCLASS_CHECK_FORBIDDEN:
            with self.assertRaises(TypeError):
                issubclass(TypeAlias, Employee)

    def test_cannot_subclass(self):
        with self.assertRaises(TypeError):
            class C(TypeAlias):
                pass

        if SUBCLASS_CHECK_FORBIDDEN:
            with self.assertRaises(TypeError):
                class C(type(TypeAlias)):
                    pass

    def test_repr(self):
        if hasattr(typing, 'TypeAlias'):
            self.assertEqual(repr(TypeAlias), 'typing.TypeAlias')
        else:
            self.assertEqual(repr(TypeAlias), 'typing_extensions.TypeAlias')

    def test_cannot_subscript(self):
        with self.assertRaises(TypeError):
            TypeAlias[int]


class AllTests(BaseTestCase):

    def test_typing_extensions_includes_standard(self):
        a = typing_extensions.__all__
        self.assertIn('ClassVar', a)
        self.assertIn('Type', a)
        self.assertIn('ChainMap', a)
        self.assertIn('ContextManager', a)
        self.assertIn('Counter', a)
        self.assertIn('DefaultDict', a)
        self.assertIn('Deque', a)
        self.assertIn('NewType', a)
        self.assertIn('overload', a)
        self.assertIn('Text', a)
        self.assertIn('TYPE_CHECKING', a)
        if TYPING_3_5_3:
            self.assertIn('Annotated', a)
        if PEP_560:
            self.assertIn('get_type_hints', a)

        if ASYNCIO:
            self.assertIn('Awaitable', a)
            self.assertIn('AsyncIterator', a)
            self.assertIn('AsyncIterable', a)
            self.assertIn('Coroutine', a)
            self.assertIn('AsyncContextManager', a)

        if PY36:
            self.assertIn('AsyncGenerator', a)

        if TYPING_3_5_3:
            self.assertIn('Protocol', a)
            self.assertIn('runtime', a)

    def test_typing_extensions_defers_when_possible(self):
        exclude = {
            'overload',
            'Text',
            'TypedDict',
            'TYPE_CHECKING',
            'Final',
            'get_type_hints'
        }
        if sys.version_info[:2] == (3, 8):
            exclude |= {'get_args', 'get_origin'}
        for item in typing_extensions.__all__:
            if item not in exclude and hasattr(typing, item):
                self.assertIs(
                    getattr(typing_extensions, item),
                    getattr(typing, item))

    def test_typing_extensions_compiles_with_opt(self):
        file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                 'typing_extensions.py')
        try:
            subprocess.check_output('{} -OO {}'.format(sys.executable,
                                                       file_path),
                                    stderr=subprocess.STDOUT,
                                    shell=True)
        except subprocess.CalledProcessError:
            self.fail('Module does not compile with optimize=2 (-OO flag).')


if __name__ == '__main__':
    main()
