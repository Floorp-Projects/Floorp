import os
import re
import shutil
import sys

if sys.version_info[:2] < (2, 6):
    sys.exit('virtualenv requires Python 2.6 or higher.')

try:
    from setuptools import setup
    from setuptools.command.test import test as TestCommand

    class PyTest(TestCommand):
        user_options = [('pytest-args=', 'a', "Arguments to pass to py.test")]

        def initialize_options(self):
            TestCommand.initialize_options(self)
            self.pytest_args = []

        def finalize_options(self):
            TestCommand.finalize_options(self)
            #self.test_args = []
            #self.test_suite = True

        def run_tests(self):
            # import here, because outside the eggs aren't loaded
            import pytest
            sys.exit(pytest.main(self.pytest_args))

    setup_params = {
        'entry_points': {
            'console_scripts': ['virtualenv=virtualenv:main'],
        },
        'zip_safe': False,
        'cmdclass': {'test': PyTest},
        'tests_require': ['pytest', 'mock'],
    }
except ImportError:
    from distutils.core import setup
    if sys.platform == 'win32':
        print('Note: without Setuptools installed you will '
              'have to use "python -m virtualenv ENV"')
        setup_params = {}
    else:
        script = 'scripts/virtualenv'
        setup_params = {'scripts': [script]}


def read_file(*paths):
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, *paths)) as f:
        return f.read()

# Get long_description from index.rst:
long_description = read_file('docs', 'index.rst')
long_description = long_description.strip().split('split here', 1)[0]
# Add release history
changes = read_file('docs', 'changes.rst')
# Only report last two releases for brevity
releases_found = 0
change_lines = []
for line in changes.splitlines():
    change_lines.append(line)
    if line.startswith('--------------'):
        releases_found += 1
    if releases_found > 2:
        break

changes = '\n'.join(change_lines[:-2]) + '\n'
changes += '`Full Changelog <https://virtualenv.pypa.io/en/latest/changes.html>`_.'
# Replace issue/pull directives
changes = re.sub(r':pull:`(\d+)`', r'PR #\1', changes)
changes = re.sub(r':issue:`(\d+)`', r'#\1', changes)

long_description += '\n\n' + changes


def get_version():
    version_file = read_file('virtualenv.py')
    version_match = re.search(r"^__version__ = ['\"]([^'\"]*)['\"]",
                              version_file, re.M)
    if version_match:
        return version_match.group(1)
    raise RuntimeError("Unable to find version string.")


# Hack to prevent stupid TypeError: 'NoneType' object is not callable error on
# exit of python setup.py test # in multiprocessing/util.py _exit_function when
# running python setup.py test (see
# http://www.eby-sarna.com/pipermail/peak/2010-May/003357.html)
try:
    import multiprocessing  # noqa
except ImportError:
    pass

setup(
    name='virtualenv',
    version=get_version(),
    description="Virtual Python Environment builder",
    long_description=long_description,
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
    ],
    keywords='setuptools deployment installation distutils',
    author='Ian Bicking',
    author_email='ianb@colorstudy.com',
    maintainer='Jannis Leidel, Carl Meyer and Brian Rosner',
    maintainer_email='python-virtualenv@groups.google.com',
    url='https://virtualenv.pypa.io/',
    license='MIT',
    py_modules=['virtualenv'],
    packages=['virtualenv_support'],
    package_data={'virtualenv_support': ['*.whl']},
    **setup_params)
