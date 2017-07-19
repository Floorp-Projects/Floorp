# flake8: noqa

try:
    from schema_builder import *
    from validators import *
    from util import *
    from error import *
except ImportError:
    from .schema_builder import *
    from .validators import *
    from .util import *
    from .error import *

__version__ = '0.10.5'
__author__ = 'tusharmakkar08'
