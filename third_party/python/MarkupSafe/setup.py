from __future__ import print_function

import io
import re
import sys
from distutils.errors import CCompilerError
from distutils.errors import DistutilsExecError
from distutils.errors import DistutilsPlatformError

from setuptools import Extension
from setuptools import find_packages
from setuptools import setup
from setuptools.command.build_ext import build_ext

with io.open("README.rst", "rt", encoding="utf8") as f:
    readme = f.read()

with io.open("src/markupsafe/__init__.py", "rt", encoding="utf8") as f:
    version = re.search(r'__version__ = "(.*?)"', f.read()).group(1)

is_jython = "java" in sys.platform
is_pypy = hasattr(sys, "pypy_version_info")

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
        version=version,
        url="https://palletsprojects.com/p/markupsafe/",
        project_urls={
            "Documentation": "https://markupsafe.palletsprojects.com/",
            "Code": "https://github.com/pallets/markupsafe",
            "Issue tracker": "https://github.com/pallets/markupsafe/issues",
        },
        license="BSD-3-Clause",
        author="Armin Ronacher",
        author_email="armin.ronacher@active-4.com",
        maintainer="The Pallets Team",
        maintainer_email="contact@palletsprojects.com",
        description="Safely add untrusted strings to HTML/XML markup.",
        long_description=readme,
        classifiers=[
            "Development Status :: 5 - Production/Stable",
            "Environment :: Web Environment",
            "Intended Audience :: Developers",
            "License :: OSI Approved :: BSD License",
            "Operating System :: OS Independent",
            "Programming Language :: Python",
            "Programming Language :: Python :: 2",
            "Programming Language :: Python :: 2.7",
            "Programming Language :: Python :: 3",
            "Programming Language :: Python :: 3.4",
            "Programming Language :: Python :: 3.5",
            "Programming Language :: Python :: 3.6",
            "Programming Language :: Python :: 3.7",
            "Topic :: Internet :: WWW/HTTP :: Dynamic Content",
            "Topic :: Software Development :: Libraries :: Python Modules",
            "Topic :: Text Processing :: Markup :: HTML",
        ],
        packages=find_packages("src"),
        package_dir={"": "src"},
        include_package_data=True,
        python_requires=">=2.7,!=3.0.*,!=3.1.*,!=3.2.*,!=3.3.*",
        cmdclass={"build_ext": ve_build_ext},
        ext_modules=ext_modules if with_binary else [],
    )


def show_message(*lines):
    print("=" * 74)
    for line in lines:
        print(line)
    print("=" * 74)


if not (is_pypy or is_jython):
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
