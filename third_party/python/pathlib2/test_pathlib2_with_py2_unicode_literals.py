
import __future__
import os
import sys
import types


def compile_source_file(source_file, flags):
    with open(source_file, "r") as f:
        source = f.read()
    return compile(source, os.path.basename(source_file), 'exec', flags)


if __name__ == "__main__":
    # Compile and run test_pathlib.py as if
    # "from __future__ import unicode_literals" had been added at the top.
    flags = __future__.CO_FUTURE_UNICODE_LITERALS
    code = compile_source_file("test_pathlib2.py", flags)
    mod = types.ModuleType('test_pathlib2')
    mod.__file__ = "test_pathlib2.py"
    sys.modules[mod.__name__] = mod
    # hack six.u() not to try to decode the string
    import six
    six.u = lambda s: s
    eval(code, mod.__dict__)
    mod.main()
