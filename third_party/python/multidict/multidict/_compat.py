import os
import platform

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))

PYPY = platform.python_implementation() == "PyPy"

USE_CYTHON_EXTENSIONS = USE_CYTHON = not NO_EXTENSIONS and not PYPY

if USE_CYTHON_EXTENSIONS:
    try:
        from . import _multidict  # noqa
    except ImportError:
        USE_CYTHON_EXTENSIONS = USE_CYTHON = False
