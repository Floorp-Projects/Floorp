import codecs
import os
import platform
import re
import sys

from setuptools import Extension, setup

NO_EXTENSIONS = bool(os.environ.get("MULTIDICT_NO_EXTENSIONS"))

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True

CFLAGS = ["-O2"]
# CFLAGS = ['-g']
if platform.system() != "Windows":
    CFLAGS.extend(
        [
            "-std=c99",
            "-Wall",
            "-Wsign-compare",
            "-Wconversion",
            "-fno-strict-aliasing",
            "-pedantic",
        ]
    )

extensions = [
    Extension(
        "multidict._multidict",
        ["multidict/_multidict.c"],
        extra_compile_args=CFLAGS,
    ),
]


with codecs.open(
    os.path.join(
        os.path.abspath(os.path.dirname(__file__)), "multidict", "__init__.py"
    ),
    "r",
    "latin1",
) as fp:
    try:
        version = re.findall(r'^__version__ = "([^"]+)"\r?$', fp.read(), re.M)[0]
    except IndexError:
        raise RuntimeError("Unable to determine version.")


def read(f):
    return open(os.path.join(os.path.dirname(__file__), f)).read().strip()


args = dict(
    name="multidict",
    version=version,
    description=("multidict implementation"),
    long_description=read("README.rst"),
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Intended Audience :: Developers",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Development Status :: 5 - Production/Stable",
    ],
    author="Andrew Svetlov",
    author_email="andrew.svetlov@gmail.com",
    url="https://github.com/aio-libs/multidict",
    project_urls={
        "Chat: Gitter": "https://gitter.im/aio-libs/Lobby",
        "CI: Azure Pipelines": "https://dev.azure.com/aio-libs/multidict/_build",
        "Coverage: codecov": "https://codecov.io/github/aio-libs/multidict",
        "Docs: RTD": "https://multidict.readthedocs.io",
        "GitHub: issues": "https://github.com/aio-libs/multidict/issues",
        "GitHub: repo": "https://github.com/aio-libs/multidict",
    },
    license="Apache 2",
    packages=["multidict"],
    python_requires=">=3.6",
    include_package_data=True,
)

if not NO_EXTENSIONS:
    print("*********************")
    print("* Accelerated build *")
    print("*********************")
    setup(ext_modules=extensions, **args)
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup(**args)
