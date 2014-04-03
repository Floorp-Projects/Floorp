from setuptools import setup

import os, sys, re

os.environ['COPYFILE_DISABLE'] = 'true'  # this disables including resource forks in tar files on os x


extra = {}
if sys.version_info >= (3,0):
    extra['use_2to3'] = True

setup(
    name="jsmin",
    version=re.search(r'__version__ = ["\']([^"\']+)', open('jsmin/__init__.py').read()).group(1),
    packages=['jsmin'],
    description='JavaScript minifier.',
    author='Dave St.Germain',
    author_email='dave@st.germa.in',
    test_suite='jsmin.test.JsTests',
    license='MIT License',
    url='https://bitbucket.org/dcs/jsmin/',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Environment :: Web Environment',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Topic :: Internet :: WWW/HTTP :: Dynamic Content',
        'Topic :: Software Development :: Pre-processors',
        'Topic :: Text Processing :: Filters',
    ],
    **extra
)
