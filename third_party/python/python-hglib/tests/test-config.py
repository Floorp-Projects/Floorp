from tests import common
import os, hglib
from hglib.util import b

class test_config(common.basetest):
    def setUp(self):
        common.basetest.setUp(self)
        f = open('.hg/hgrc', 'a')
        f.write('[section]\nkey=value\n')
        f.close()
        self.client = hglib.open()

    def test_basic(self):
        config = self.client.config()

        self.assertTrue(
                (b('section'), b('key'), b('value')) in self.client.config())

        self.assertTrue([(b('section'), b('key'), b('value'))],
                        self.client.config(b('section')))
        self.assertTrue([(b('section'), b('key'), b('value'))],
                        self.client.config([b('section'), b('foo')]))
        self.assertRaises(hglib.error.CommandError,
                          self.client.config, [b('a.b'), b('foo')])

    def test_show_source(self):
        config = self.client.config(showsource=True)

        self.assertTrue((os.path.abspath(b('.hg/hgrc')) + b(':2'),
                         b('section'), b('key'), b('value')) in config)

class test_config_arguments(common.basetest):
    def test_basic(self):
        client = hglib.open(configs=[b('diff.unified=5'), b('a.b=foo')])
        self.assertEqual(client.config(b('a')), [(b('a'), b('b'), b('foo'))])
        self.assertEqual(client.config(b('diff')),
                         [(b('diff'), b('unified'), b('5'))])
