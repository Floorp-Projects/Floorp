import os
from nose.plugins import Plugin

class WithHgPlugin(Plugin):
    name = 'with-hg'
    enabled = False

    def options(self, parser, env):
        Plugin.options(self, parser, env)
        parser.add_option('--with-hg',
                          action='store',
                          type='string',
                          metavar='HG',
                          dest='with_hg',
                          help='test using specified hg script.')

    def configure(self, options, conf):
        Plugin.configure(self, options, conf)
        if options.with_hg:
            self.enabled = True
            self.hgpath = os.path.realpath(options.with_hg)

    def begin(self):
        import hglib

        p = hglib.util.popen([self.hgpath, 'version'])
        p.communicate()

        if p.returncode:
            raise ValueError("custom hg %r doesn't look like Mercurial"
                             % self.hgpath)

        hglib.HGPATH = self.hgpath
