import os
from setuptools import setup, Extension
import sys
import platform
import warnings
import codecs
from distutils.command.build_ext import build_ext
from distutils.errors import CCompilerError
from distutils.errors import DistutilsPlatformError, DistutilsExecError
from _pyrsistent_version import __version__

readme_path = os.path.join(os.path.dirname(__file__), 'README.rst')
with codecs.open(readme_path, encoding='utf8') as f:
    readme = f.read()

extensions = []
if platform.python_implementation() == 'CPython':
    extensions = [Extension('pvectorc', sources=['pvectorcmodule.c'])]

needs_pytest = {'pytest', 'test', 'ptr'}.intersection(sys.argv)
pytest_runner = ['pytest-runner'] if needs_pytest else []


class custom_build_ext(build_ext):
    """Allow C extension building to fail."""

    warning_message = """
********************************************************************************
WARNING: Could not build the %s.
         Pyrsistent will still work but performance may be degraded.
         %s
********************************************************************************
"""

    def run(self):
        try:
            build_ext.run(self)
        except Exception:
            e = sys.exc_info()[1]
            sys.stderr.write('%s\n' % str(e))
            sys.stderr.write(self.warning_message % ("extension modules", "There was an issue with your platform configuration - see above."))

    def build_extension(self, ext):
        name = ext.name
        try:
            build_ext.build_extension(self, ext)
        except Exception:
            e = sys.exc_info()[1]
            sys.stderr.write('%s\n' % str(e))
            sys.stderr.write(self.warning_message % ("%s extension module" % name, "The output above this warning shows how the compilation failed."))

setup(
    name='pyrsistent',
    version=__version__,
    description='Persistent/Functional/Immutable data structures',
    long_description=readme,
    author='Tobias Gustafsson',
    author_email='tobias.l.gustafsson@gmail.com',
    url='http://github.com/tobgu/pyrsistent/',
    license='MIT',
    py_modules=['_pyrsistent_version'],
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: Implementation :: PyPy',
    ],
    test_suite='tests',
    tests_require=['pytest<5', 'hypothesis<5'],
    scripts=[],
    setup_requires=pytest_runner,
    ext_modules=extensions,
    cmdclass={'build_ext': custom_build_ext},
    install_requires=['six'],
    packages=['pyrsistent'],
    package_data={'pyrsistent': ['py.typed', '__init__.pyi', 'typing.pyi']},
)
