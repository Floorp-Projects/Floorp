import os
import re
import shutil
import sys

try:
    from setuptools import setup
    setup_params = {
        'entry_points': {
            'console_scripts': [
                'virtualenv=virtualenv:main',
                'virtualenv-%s.%s=virtualenv:main' % sys.version_info[:2]
            ],
        },
        'zip_safe': False,
        'test_suite': 'nose.collector',
        'tests_require': ['nose', 'Mock'],
    }
except ImportError:
    from distutils.core import setup
    if sys.platform == 'win32':
        print('Note: without Setuptools installed you will have to use "python -m virtualenv ENV"')
        setup_params = {}
    else:
        script = 'scripts/virtualenv'
        script_ver = script + '-%s.%s' % sys.version_info[:2]
        shutil.copy(script, script_ver)
        setup_params = {'scripts': [script, script_ver]}

here = os.path.dirname(os.path.abspath(__file__))

## Get long_description from index.txt:
f = open(os.path.join(here, 'docs', 'index.rst'))
long_description = f.read().strip()
long_description = long_description.split('split here', 1)[1]
f.close()
f = open(os.path.join(here, 'docs', 'news.rst'))
long_description += "\n\n" + f.read()
f.close()


def get_version():
    f = open(os.path.join(here, 'virtualenv.py'))
    version_file = f.read()
    f.close()
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
    import multiprocessing
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
        'Programming Language :: Python :: 2.5',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.1',
        'Programming Language :: Python :: 3.2',
    ],
    keywords='setuptools deployment installation distutils',
    author='Ian Bicking',
    author_email='ianb@colorstudy.com',
    maintainer='Jannis Leidel, Carl Meyer and Brian Rosner',
    maintainer_email='python-virtualenv@groups.google.com',
    url='http://www.virtualenv.org',
    license='MIT',
    py_modules=['virtualenv'],
    packages=['virtualenv_support'],
    package_data={'virtualenv_support': ['*-py%s.egg' % sys.version[:3], '*.tar.gz']},
    **setup_params)
