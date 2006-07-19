from distutils.core import setup
setup(name="l10n-tools",
      version="0.1",
      author="Axel Hecht",
      author_email="l10n@mozilla.com",
      scripts=['scripts/compare-locales'],
      package_dir={'': 'lib'},
      packages=['Mozilla']
      )
