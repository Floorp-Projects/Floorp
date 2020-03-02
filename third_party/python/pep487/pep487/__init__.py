# -*- coding: utf-8 -*-
#
# Copyright (C) 2017 by Gregor Giesen
#
# This is a backport of PEP487's simpler customisation of class
# creation by Martin Teichmann <https://www.python.org/dev/peps/pep-0487/>
# for Python versions before 3.6.
#
# PEP487 is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published
# by the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# PEP487 is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PEP487. If not, see <http://www.gnu.org/licenses/>.
#
"""pep487.py: Simpler customisation of class creation"""

import abc
import sys
import types

__all__ = ('PEP487Meta', 'PEP487Object', 'ABCMeta', 'ABC')

HAS_PY36 = sys.version_info >= (3, 6)
HAS_PEP487 = HAS_PY36

if HAS_PEP487:
    PEP487Meta = type         # pragma: no cover
    ABCMeta = abc.ABCMeta     # pragma: no cover
    ABC = abc.ABC             # pragma: no cover
    PEP487Base = object       # pragma: no cover
    PEP487Object = object     # pragma: no cover
else:
    class PEP487Meta(type):
        def __new__(mcls, name, bases, ns, **kwargs):
            init = ns.get('__init_subclass__')
            if isinstance(init, types.FunctionType):
                ns['__init_subclass__'] = classmethod(init)
            cls = super().__new__(mcls, name, bases, ns)
            for key, value in cls.__dict__.items():
                func = getattr(value, '__set_name__', None)
                if func is not None:
                    func(cls, key)
            super(cls, cls).__init_subclass__(**kwargs)
            return cls

        def __init__(cls, name, bases, ns, **kwargs):
            super().__init__(name, bases, ns)

    class ABCMeta(abc.ABCMeta):
        def __new__(mcls, name, bases, ns, **kwargs):
            init = ns.get('__init_subclass__')
            if isinstance(init, types.FunctionType):
                ns['__init_subclass__'] = classmethod(init)
            cls = super().__new__(mcls, name, bases, ns)
            for key, value in cls.__dict__.items():
                func = getattr(value, '__set_name__', None)
                if func is not None:
                    func(cls, key)
            super(cls, cls).__init_subclass__(**kwargs)
            return cls

        def __init__(cls, name, bases, ns, **kwargs):
            super().__init__(name, bases, ns)

    class PEP487Base:
        @classmethod
        def __init_subclass__(cls, **kwargs):
            pass

    class PEP487Object(PEP487Base, metaclass=PEP487Meta):
        pass

    class ABC(PEP487Base, metaclass=ABCMeta):
        pass
