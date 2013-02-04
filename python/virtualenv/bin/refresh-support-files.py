#!/usr/bin/env python
"""
Refresh any files in ../virtualenv_support/ that come from elsewhere
"""

import os
try:
    from urllib.request import urlopen
except ImportError:
    from urllib2 import urlopen
import sys

here = os.path.dirname(__file__)
support_location = os.path.join(here, '..', 'virtualenv_support')
embedded_location = os.path.join(here, '..', 'virtualenv_embedded')

embedded_files = [
    ('http://peak.telecommunity.com/dist/ez_setup.py', 'ez_setup.py'),
    ('http://python-distribute.org/distribute_setup.py', 'distribute_setup.py'),
]

support_files = [
    ('http://pypi.python.org/packages/2.6/s/setuptools/setuptools-0.6c11-py2.6.egg', 'setuptools-0.6c11-py2.6.egg'),
    ('http://pypi.python.org/packages/2.5/s/setuptools/setuptools-0.6c11-py2.5.egg', 'setuptools-0.6c11-py2.5.egg'),
    ('http://pypi.python.org/packages/source/d/distribute/distribute-0.6.31.tar.gz', 'distribute-0.6.31.tar.gz'),
    ('http://pypi.python.org/packages/source/p/pip/pip-1.2.1.tar.gz', 'pip-1.2.1.tar.gz'),
]


def refresh_files(files, location):
    for url, filename in files:
        sys.stdout.write('fetching %s ... ' % url)
        sys.stdout.flush()
        f = urlopen(url)
        content = f.read()
        f.close()
        print('done.')
        filename = os.path.join(location, filename)
        if os.path.exists(filename):
            f = open(filename, 'rb')
            cur_content = f.read()
            f.close()
        else:
            cur_content = ''
        if cur_content == content:
            print('  %s up-to-date' % filename)
        else:
            print('  overwriting %s' % filename)
            f = open(filename, 'wb')
            f.write(content)
            f.close()


def main():
    refresh_files(embedded_files, embedded_location)
    refresh_files(support_files, support_location)

if __name__ == '__main__':
    main()
