import sys, os
try:
    from setuptools import setup
    kw = {'entry_points':
          """[console_scripts]\nvirtualenv = virtualenv:main\n""",
          'zip_safe': False}
except ImportError:
    from distutils.core import setup
    if sys.platform == 'win32':
        print 'Note: without Setuptools installed you will have to use "python -m virtualenv ENV"'
    else:
        kw = {'scripts': ['scripts/virtualenv']}
import re

here = os.path.dirname(os.path.abspath(__file__))

## Figure out the version from virtualenv.py:
version_re = re.compile(
    r'virtualenv_version = "(.*?)"')
fp = open(os.path.join(here, 'virtualenv.py'))
version = None
for line in fp:
    match = version_re.search(line)
    if match:
        version = match.group(1)
        break
else:
    raise Exception("Cannot find version in virtualenv.py")
fp.close()

## Get long_description from index.txt:
f = open(os.path.join(here, 'docs', 'index.txt'))
long_description = f.read().strip()
long_description = long_description.split('split here', 1)[1]
f.close()

## A warning just for Ian (related to distribution):
try:
    import getpass
except ImportError:
    is_ianb = False
else:
    is_ianb = getpass.getuser() == 'ianb'

if is_ianb and 'register' in sys.argv:
    if 'hg tip\n~~~~~~' in long_description:
        print >> sys.stderr, (
            "WARNING: hg tip is in index.txt")

setup(name='virtualenv',
      version=version,
      description="Virtual Python Environment builder",
      long_description=long_description,
      classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
      ],
      keywords='setuptools deployment installation distutils',
      author='Ian Bicking',
      author_email='ianb@colorstudy.com',
      url='http://virtualenv.openplans.org',
      license='MIT',
      py_modules=['virtualenv'],
      packages=['virtualenv_support'],
      package_data={'virtualenv_support': ['*-py%s.egg' % sys.version[:3], '*.tar.gz']},
      **kw
      )
