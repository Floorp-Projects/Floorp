from marionette import MarionetteTestCase
from marionette_driver.errors import NoSuchElementException
import threading
import SimpleHTTPServer
import SocketServer
import BaseHTTPServer
import urllib
import urlparse
import os
import posixpath

DEBUG = True

# Example taken from mozilla-central/browser/components/loop/

# XXX Once we're on a branch with bug 993478 landed, we may want to get
# rid of this HTTP server and just use the built-in one from Marionette,
# since there will less code to maintain, and it will be faster.  We'll
# need to consider whether this code wants to be shared with WebDriver tests
# for other browsers, though.

class ThreadingSimpleServer(SocketServer.ThreadingMixIn,
                            BaseHTTPServer.HTTPServer):
    pass


class HttpRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler, object):
    def __init__(self, *args):
        # set root to toolkit/components/microformats/
        self.root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.normpath(__file__))))
        super(HttpRequestHandler, self).__init__(*args)

    def log_message(self, format, *args, **kwargs):
        if DEBUG:
            super(HttpRequestHandler, self).log_message(format, *args, **kwargs)

    def translate_path(self, path):
        """Translate a /-separated PATH to the local filename syntax.

        Components that mean special things to the local file system
        (e.g. drive or directory names) are ignored.  (XXX They should
        probably be diagnosed.)

        """
        # abandon query parameters
        path = path.split('?',1)[0]
        path = path.split('#',1)[0]
        # Don't forget explicit trailing slash when normalizing. Issue17324
        trailing_slash = path.rstrip().endswith('/')
        path = posixpath.normpath(urllib.unquote(path))
        words = path.split('/')
        words = filter(None, words)
        path = self.root
        for word in words:
            drive, word = os.path.splitdrive(word)
            head, word = os.path.split(word)
            if word in (os.curdir, os.pardir): continue
            path = os.path.join(path, word)
        if trailing_slash:
            path += '/'
        return path

class BaseTestFrontendUnits(MarionetteTestCase):

    @classmethod
    def setUpClass(cls):
        super(BaseTestFrontendUnits, cls).setUpClass()

        # Port 0 means to select an arbitrary unused port
        cls.server = ThreadingSimpleServer(('', 0), HttpRequestHandler)
        cls.ip, cls.port = cls.server.server_address

        cls.server_thread = threading.Thread(target=cls.server.serve_forever)
        cls.server_thread.daemon = False
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server_thread.join()

        # make sure everything gets GCed so it doesn't interfere with the next
        # test class.  Even though this is class-static, each subclass gets
        # its own instance of this stuff.
        cls.server_thread = None
        cls.server = None

    def setUp(self):
        super(BaseTestFrontendUnits, self).setUp()

        # Unfortunately, enforcing preferences currently comes with the side
        # effect of launching and restarting the browser before running the
        # real functional tests.  Bug 1048554 has been filed to track this.
        #
        # Note: when e10s is enabled by default, this pref can go away. The automatic
        # restart will also go away if this is still the only pref set here.
        self.marionette.enforce_gecko_prefs({
            "browser.tabs.remote.autostart": True
        })

        # This extends the timeout for find_element. We need this as the tests
        # take an amount of time to run after loading, which we have to wait for.
        self.marionette.timeout.implicit = 120

        self.marionette.timeout.page_load = 120

    # srcdir_path should be the directory relative to this file.
    def set_server_prefix(self, srcdir_path):
        self.server_prefix = urlparse.urljoin("http://localhost:" + str(self.port),
                                              srcdir_path)

    def check_page(self, page):

        self.marionette.navigate(urlparse.urljoin(self.server_prefix, page))
        try:
            self.marionette.find_element("id", 'complete')
        except NoSuchElementException:
            fullPageUrl = urlparse.urljoin(self.relPath, page)

            details = "%s: 1 failure encountered\n%s" % \
                      (fullPageUrl,
                       self.get_failure_summary(
                           fullPageUrl, "Waiting for Completion",
                           "Could not find the test complete indicator"))

            raise AssertionError(details)

        fail_node = self.marionette.find_element("css selector",
                                                 '.failures > em')
        if fail_node.text == "0":
            return

        # This may want to be in a more general place triggerable by an env
        # var some day if it ends up being something we need often:
        #
        # If you have browser-based unit tests which work when loaded manually
        # but not from marionette, uncomment the two lines below to break
        # on failing tests, so that the browsers won't be torn down, and you
        # can use the browser debugging facilities to see what's going on.
        #from ipdb import set_trace
        #set_trace()

        raise AssertionError(self.get_failure_details(page))

    def get_failure_summary(self, fullPageUrl, testName, testError):
        return "TEST-UNEXPECTED-FAIL | %s | %s - %s" % (fullPageUrl, testName, testError)

    def get_failure_details(self, page):
        fail_nodes = self.marionette.find_elements("css selector",
                                                   '.test.fail')
        fullPageUrl = urlparse.urljoin(self.relPath, page)

        details = ["%s: %d failure(s) encountered:" % (fullPageUrl, len(fail_nodes))]

        for node in fail_nodes:
            errorText = node.find_element("css selector", '.error').text

            # We have to work our own failure message here, as we could be reporting multiple failures.
            # XXX Ideally we'd also give the full test tree for <test name> - that requires walking
            # up the DOM tree.

            # Format: TEST-UNEXPECTED-FAIL | <filename> | <test name> - <test error>
            details.append(
                self.get_failure_summary(page,
                                         node.find_element("tag name", 'h2').text.split("\n")[0],
                                         errorText.split("\n")[0]))
            details.append(
                errorText)
        return "\n".join(details)
