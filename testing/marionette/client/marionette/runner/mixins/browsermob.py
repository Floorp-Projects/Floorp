import os

from browsermobproxy import Server
from marionette import MarionetteTestCase


class BrowserMobProxyOptionsMixin(object):

    # verify_usage
    def browsermob_verify_usage(self, options, tests):
        if options.browsermob_script is not None:
            if not os.path.exists(options.browsermob_script):
                raise ValueError('%s not found' % options.browsermob_script)

    def __init__(self, **kwargs):
        # Inheriting object must call this __init__ to set up option handling
        group = self.add_option_group('Browsermob Proxy')
        group.add_option('--browsermob-script',
                         action='store',
                         dest='browsermob_script',
                         type='string',
                         help='path to the browsermob-proxy shell script or batch file')
        group.add_option('--browsermob-port',
                         action='store',
                         dest='browsermob_port',
                         type='int',
                         help='port to run the browsermob proxy on')
        self.verify_usage_handlers.append(self.browsermob_verify_usage)


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
