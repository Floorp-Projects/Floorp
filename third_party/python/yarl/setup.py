import pathlib
import os
import sys
import re

from setuptools import setup, Extension


if sys.version_info < (3, 5):
    raise RuntimeError("yarl 1.4+ requires Python 3.5+")


NO_EXTENSIONS = bool(os.environ.get("YARL_NO_EXTENSIONS"))  # type: bool

if sys.implementation.name != "cpython":
    NO_EXTENSIONS = True


extensions = [Extension("yarl._quoting_c", ["yarl/_quoting_c.c"])]
# extra_compile_args=["-g"],
# extra_link_args=["-g"],


here = pathlib.Path(__file__).parent
fname = here / "yarl" / "__init__.py"

with fname.open(encoding="utf8") as fp:
    try:
        version = re.findall(r'^__version__ = "([^"]+)"$', fp.read(), re.M)[0]
    except IndexError:
        raise RuntimeError("Unable to determine version.")

install_requires = [
    "multidict>=4.0",
    "idna>=2.0",
    'typing_extensions>=3.7.4;python_version<"3.8"',
]


def read(name):
    fname = here / name
    with fname.open(encoding="utf8") as f:
        return f.read()


args = dict(
    name="yarl",
    version=version,
    description=("Yet another URL library"),
    long_description="\n\n".join([read("README.rst"), read("CHANGES.rst")]),
    long_description_content_type="text/x-rst",
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Intended Audience :: Developers",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Topic :: Internet :: WWW/HTTP",
    ],
    author="Andrew Svetlov",
    author_email="andrew.svetlov@gmail.com",
    url="https://github.com/aio-libs/yarl/",
    license="Apache 2",
    packages=["yarl"],
    install_requires=install_requires,
    python_requires=">=3.6",
    include_package_data=True,
)


if not NO_EXTENSIONS:
    print("**********************")
    print("* Accellerated build *")
    print("**********************")
    setup(ext_modules=extensions, **args)
else:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    setup(**args)
