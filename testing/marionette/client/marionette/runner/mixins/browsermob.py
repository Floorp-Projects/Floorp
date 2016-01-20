# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from browsermobproxy import Server
from marionette import MarionetteTestCase


class BrowserMobProxyArguments(object):
    name = 'Browsermob Proxy'
    args = [
        [['--browsermob-script'],
         {'help': 'path to the browsermob-proxy shell script or batch file',
          }],
        [['--browsermob-port'],
         {'type': int,
          'help': 'port to run the browsermob proxy on',
          }],
    ]

    def verify_usage_handler(self, args):
        if args.browsermob_script is not None:
            if not os.path.exists(args.browsermob_script):
                raise ValueError('%s not found' % args.browsermob_script)


class BrowserMobProxyTestCaseMixin(object):

    def __init__(self, *args, **kwargs):
        self.browsermob_server = None
        self.browsermob_port = kwargs.pop('browsermob_port')
        self.browsermob_script = kwargs.pop('browsermob_script')

    def setUp(self):
        options = {}
        if self.browsermob_port:
            options['port'] = self.browsermob_port
        if not self.browsermob_script:
            raise ValueError('Must specify --browsermob-script in order to '
                             'run browsermobproxy tests')
        self.browsermob_server = Server(
            self.browsermob_script, options=options)
        self.browsermob_server.start()

    def create_browsermob_proxy(self):
        client = self.browsermob_server.create_proxy()
        with self.marionette.using_context('chrome'):
            self.marionette.execute_script("""
                Services.prefs.setIntPref('network.proxy.type', 1);
                Services.prefs.setCharPref('network.proxy.http', 'localhost');
                Services.prefs.setIntPref('network.proxy.http_port', %(port)s);
                Services.prefs.setCharPref('network.proxy.ssl', 'localhost');
                Services.prefs.setIntPref('network.proxy.ssl_port', %(port)s);
            """ % {"port": client.port})
        return client

    def tearDown(self):
        if self.browsermob_server:
            self.browsermob_server.stop()
            self.browsermob_server = None

    __del__ = tearDown


class BrowserMobTestCase(MarionetteTestCase, BrowserMobProxyTestCaseMixin):

    def __init__(self, *args, **kwargs):
        MarionetteTestCase.__init__(self, *args, **kwargs)
        BrowserMobProxyTestCaseMixin.__init__(self, *args, **kwargs)

    def setUp(self):
        MarionetteTestCase.setUp(self)
        BrowserMobProxyTestCaseMixin.setUp(self)

    def tearDown(self):
        BrowserMobProxyTestCaseMixin.tearDown(self)
        MarionetteTestCase.tearDown(self)
