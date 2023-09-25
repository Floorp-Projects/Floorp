import os
import pathlib
import re
import sys

from setuptools import Extension, setup

if sys.version_info < (3, 6):
    raise RuntimeError("frozenlist 1.x requires Python 3.6+")


NO_EXTENSIONS = (
    bool(os.environ.get('FROZENLIST_NO_EXTENSIONS')) or
    sys.implementation.name != "cpython"
)

here = pathlib.Path(__file__).parent

if NO_EXTENSIONS:
    print("*********************")
    print("* Pure Python build *")
    print("*********************")
    ext_modules = None
else:
    print("**********************")
    print("* Accellerated build *")
    print("**********************")
    ext_modules = [
        Extension('frozenlist._frozenlist', ['frozenlist/_frozenlist.c'])
    ]


txt = (here / 'frozenlist' / '__init__.py').read_text('utf-8')
try:
    version = re.findall(r"^__version__ = '([^']+)'\r?$",
                         txt, re.M)[0]
except IndexError:
    raise RuntimeError('Unable to determine version.')

install_requires = []


def read(f):
    return (here / f).read_text('utf-8').strip()


setup(
    name='frozenlist',
    version=version,
    description=(
        'A list-like structure which implements '
        'collections.abc.MutableSequence'
    ),
    long_description='\n\n'.join((read('README.rst'), read('CHANGES.rst'))),
    long_description_content_type="text/x-rst",
    classifiers=[
        'License :: OSI Approved :: Apache Software License',
        'Intended Audience :: Developers',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Development Status :: 5 - Production/Stable',
        'Operating System :: POSIX',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
    ],
    author='Nikolay Kim',
    author_email='fafhrd91@gmail.com',
    maintainer='Martijn Pieters <mj@zopatista.com>',
    maintainer_email='aio-libs@googlegroups.com',
    url='https://github.com/aio-libs/frozenlist',
    project_urls={
        'Chat: Gitter': 'https://gitter.im/aio-libs/Lobby',
        'CI: Github Actions':
            'https://github.com/aio-libs/frozenlist/actions',
        'Coverage: codecov': 'https://codecov.io/github/aio-libs/frozenlist',
        'Docs: RTD': 'https://frozenlist.readthedocs.io',
        'GitHub: issues': 'https://github.com/aio-libs/frozenlist/issues',
        'GitHub: repo': 'https://github.com/aio-libs/frozenlist',
    },
    license='Apache 2',
    packages=['frozenlist'],
    ext_modules=ext_modules,
    python_requires='>=3.6',
    install_requires=install_requires,
    include_package_data=True,
)
