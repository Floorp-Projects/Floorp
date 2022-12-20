import os
import platform
import sys
from distutils.errors import CCompilerError
from distutils.errors import DistutilsExecError
from distutils.errors import DistutilsPlatformError

from setuptools import Extension
from setuptools import setup
from setuptools.command.build_ext import build_ext

ext_modules = [Extension("markupsafe._speedups", ["src/markupsafe/_speedups.c"])]


class BuildFailed(Exception):
    pass


class ve_build_ext(build_ext):
    """This class allows C extension building to fail."""

    def run(self):
        try:
            build_ext.run(self)
        except DistutilsPlatformError:
            raise BuildFailed()

    def build_extension(self, ext):
        try:
            build_ext.build_extension(self, ext)
        except (CCompilerError, DistutilsExecError, DistutilsPlatformError):
            raise BuildFailed()
        except ValueError:
            # this can happen on Windows 64 bit, see Python issue 7511
            if "'path'" in str(sys.exc_info()[1]):  # works with Python 2 and 3
                raise BuildFailed()
            raise


def run_setup(with_binary):
    setup(
        name="MarkupSafe",
        cmdclass={"build_ext": ve_build_ext},
        ext_modules=ext_modules if with_binary else [],
    )


def show_message(*lines):
    print("=" * 74)
    for line in lines:
        print(line)
    print("=" * 74)


supports_speedups = platform.python_implementation() not in {"PyPy", "Jython"}

if os.environ.get("CIBUILDWHEEL", "0") == "1" and supports_speedups:
    run_setup(True)
elif supports_speedups:
    try:
        run_setup(True)
    except BuildFailed:
        show_message(
            "WARNING: The C extension could not be compiled, speedups"
            " are not enabled.",
            "Failure information, if any, is above.",
            "Retrying the build without the C extension now.",
        )
        run_setup(False)
        show_message(
            "WARNING: The C extension could not be compiled, speedups"
            " are not enabled.",
            "Plain-Python build succeeded.",
        )
else:
    run_setup(False)
    show_message(
        "WARNING: C extensions are not supported on this Python"
        " platform, speedups are not enabled.",
        "Plain-Python build succeeded.",
    )
