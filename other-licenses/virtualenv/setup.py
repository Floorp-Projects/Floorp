import sys, os
try:
    from setuptools import setup
    kw = {'entry_points':
          """[console_scripts]\nvirtualenv = virtualenv:main\n""",
          'zip_safe': False}
except ImportError:
    from distutils.core import setup
    if sys.platform == 'win32':
        print('Note: without Setuptools installed you will have to use "python -m virtualenv ENV"')
        kw = {}
    else:
        kw = {'scripts': ['scripts/virtualenv']}

here = os.path.dirname(os.path.abspath(__file__))

## Get long_description from index.txt:
f = open(os.path.join(here, 'docs', 'index.txt'))
long_description = f.read().strip()
long_description = long_description.split('split here', 1)[1]
f.close()
f = open(os.path.join(here, 'docs', 'news.txt'))
long_description += "\n\n" + f.read()
f.close()

setup(name='virtualenv',
      # If you change the version here, change it in virtualenv.py and
      # docs/conf.py as well
      version="1.7.1.2",
      description="Virtual Python Environment builder",
      long_description=long_description,
      classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.4',
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
      test_suite='nose.collector',
      tests_require=['nose', 'Mock'],
      **kw
      )
