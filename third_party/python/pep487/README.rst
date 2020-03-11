===============================================
PEP487: Simpler customisation of class creation
===============================================

This is a backport of PEP487's simpler customisation of class
creation by Martin Teichmann <https://www.python.org/dev/peps/pep-0487/>
for Python versions before 3.6.

PEP487 is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.


Subclass init
=============

>>> from pep487 import PEP487Object
>>> class FooBase(PEP487Object):
...     foos = set()
...
...     def __init_subclass__(cls, **kwargs):
...         cls.foos.add(cls.__name__)

Using `PEP487Object` as base class all subclasses of FooBase
will add their name to the common class variable 'foos'.

>>> class Foo1(FooBase):
...     pass
>>> class Foo2(FooBase):
...     pass

Hence:

>>> FooBase.foos
{'Foo1', 'Foo2'}



Property names and owner
========================

If a class object has a method `__set_name__` upon declaration
of an PEP487Object class, it will be called:

>>> class NamedProperty:
...     def __set_name__(self, owner, name):
...         self.context = owner
...         self.name = name

>>> class Bar(PEP487Object):
...     foo = NamedProperty()
...     bar = NamedProperty()

Consequently:

>>> Bar.foo.name is 'foo' and Bar.foo.context is Bar
True
>>> Bar.bar.name is 'bar' and Bar.bar.context is Bar
True


Abstract base classes
=====================

Since `PEP487Object` has a custom metaclass, it is incompatible
to `abc.ABC`. Therefore `pep487` contains patched versions of `ABC`
and `ABCMeta`.
