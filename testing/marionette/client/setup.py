import os
from setuptools import setup, find_packages
import sys

version = '0.7.9'

# dependencies
with open('requirements.txt') as f:
    deps = f.read().splitlines()

# Requirements.txt contains a pointer to the local copy of marionette_transport;
# if we're installing using setup.py, handle this locally or replace with a valid
# pypi package reference.
deps = [x for x in deps if 'transport' not in x]
transport_dir = os.path.join(os.path.dirname(__file__), os.path.pardir, 'transport')
method = [x for x in sys.argv if x in ('develop', 'install')]
if os.path.exists(transport_dir) and method:
    cmd = [sys.executable, 'setup.py', method[0]]
    import subprocess
    try:
        subprocess.check_call(cmd, cwd=transport_dir)
    except subprocess.CalledProcessError:
        print "Error running setup.py in %s" % transport_dir
        raise
else:
    deps += ['marionette-transport == 0.2']

setup(name='marionette_client',
      version=version,
      description="Marionette test automation client",
      long_description='See http://marionette-client.readthedocs.org/',
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='Jonathan Griffin',
      author_email='jgriffin@mozilla.com',
      url='https://wiki.mozilla.org/Auto-tools/Projects/Marionette',
      license='MPL',
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      package_data={'marionette': ['touch/*.js']},
      include_package_data=True,
      zip_safe=False,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      marionette = marionette.runtests:cli
      """,
      install_requires=deps,
      )

