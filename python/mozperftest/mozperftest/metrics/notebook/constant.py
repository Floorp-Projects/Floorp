# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
from pathlib import Path
from types import MappingProxyType

from .transforms import single_json


class Constant(object):
    """A singleton class to store all constants.
    """

    __instance = None

    def __new__(cls, *args, **kw):
        if cls.__instance is None:
            cls.__instance = object.__new__(cls, *args, **kw)
        return cls.__instance

    def __init__(self):
        self.__here = Path(os.path.dirname(os.path.abspath(__file__)))
        # XXX This needs to be more dynamic
        self.__predefined_transformers = {
            "SingleJsonRetriever": single_json.SingleJsonRetriever
        }

    @property
    def predefined_transformers(self):
        return MappingProxyType(self.__predefined_transformers).copy()

    @property
    def here(self):
        return self.__here
