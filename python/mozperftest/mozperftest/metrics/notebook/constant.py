# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import pathlib
from types import MappingProxyType

from .transformer import get_transformers


class Constant(object):
    """A singleton class to store all constants."""

    __instance = None

    def __new__(cls, *args, **kw):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls, *args, **kw)
        return cls.__instance

    def __init__(self):
        self.__here = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))
        self.__predefined_transformers = get_transformers(self.__here / "transforms")

    @property
    def predefined_transformers(self):
        return MappingProxyType(self.__predefined_transformers).copy()

    @property
    def here(self):
        return self.__here
