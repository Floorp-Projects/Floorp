# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/nedbat/coveragepy/blob/master/NOTICE.txt

"""
Imports that we need at runtime, but might not be present.

When importing one of these modules, always do it in the function where you
need the module.  Some tests will need to remove the module.  If you import
it at the top level of your module, then the test won't be able to simulate
the module being unimportable.

The import will always succeed, but the value will be None if the module is
unavailable.

Bad::

    # MyModule.py
    from coverage.optional import unsure

    def use_unsure():
        unsure.something()

Good::

    # MyModule.py

    def use_unsure():
        from coverage.optional import unsure
        if unsure is None:
            raise Exception("Module unsure isn't available!")

        unsure.something()

"""

import contextlib

# This file's purpose is to provide modules to be imported from here.
# pylint: disable=unused-import

# TOML support is an install-time extra option.
try:
    import toml
except ImportError:         # pragma: not covered
    toml = None


@contextlib.contextmanager
def without(modname):
    """Hide a module for testing.

    Use this in a test function to make an optional module unavailable during
    the test::

        with coverage.optional.without('toml'):
            use_toml_somehow()

    Arguments:
        modname (str): the name of a module importable from
            `coverage.optional`.

    """
    real_module = globals()[modname]
    try:
        globals()[modname] = None
        yield
    finally:
        globals()[modname] = real_module
