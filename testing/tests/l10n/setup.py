from distutils.core import setup

from distutils.cmd import Command
import glob

class web(Command):
  description = 'install web files'
  user_options = [('target=','d','base directory for installation')]
  
  def initialize_options(self):
    self.target = None
    pass
  def finalize_options(self):
    pass
  def run(self):
    self.ensure_dirname('target')
    for f in glob.glob('web/*.*'):
      if f.find('/CVS') >=0 or f.find('~') >= 0:
        continue
      self.copy_file(f, self.target)

setup(name="l10n-tools",
      version="0.2",
      author="Axel Hecht",
      author_email="l10n@mozilla.com",
      scripts=['scripts/compare-locales', 'scripts/verify-search',
               'scripts/test-locales',
               'scripts/verify-rss-redirects'],
      package_dir={'': 'lib'},
      packages=['Mozilla'],
      cmdclass={'web': web}
      )
