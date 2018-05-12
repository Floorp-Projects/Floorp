# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

from marionette_driver.errors import JavascriptException, ScriptTimeoutException

AWSY_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
if AWSY_PATH not in sys.path:
    sys.path.append(AWSY_PATH)

from awsy import process_perf_data, webservers
from awsy.awsy_test_case import AwsyTestCase

class TestMemoryUsage(AwsyTestCase):
    """Provides a test that collects memory usage at various checkpoints:
      - "Start" - Just after startup
      - "StartSettled" - After an additional wait time
      - "TabsOpen" - After opening all provided URLs
      - "TabsOpenSettled" - After an additional wait time
      - "TabsOpenForceGC" - After forcibly invoking garbage collection
      - "TabsClosed" - After closing all tabs
      - "TabsClosedSettled" - After an additional wait time
      - "TabsClosedForceGC" - After forcibly invoking garbage collection
    """

    def urls(self):
        return self._urls

    def perf_suites(self):
        return process_perf_data.PERF_SUITES

    def perf_checkpoints(self):
        return process_perf_data.CHECKPOINTS

    def setUp(self):
        AwsyTestCase.setUp(self)
        self.logger.info("setting up")
        self._webroot_dir = self.testvars["webRootDir"]
        self._urls = []

        urls = None
        default_tp5n_manifest = os.path.join(self._webroot_dir, 'page_load_test', 'tp5n',
                                             'tp5n.manifest')
        tp5n_manifest = self.testvars.get("pageManifest", default_tp5n_manifest)
        with open(tp5n_manifest) as fp:
            urls = fp.readlines()
        urls = map(lambda x:x.replace('localhost', 'localhost:{}'), urls)

        # We haven't set self._urls yet, so this value might be zero if
        # 'entities' wasn't specified.
        to_load = self.pages_to_load()
        if not to_load:
            to_load = len(urls)
        self._webservers = webservers.WebServers("localhost",
                                                 8001,
                                                 self._webroot_dir,
                                                 to_load)
        self._webservers.start()
        for url, server in zip(urls, self._webservers.servers):
            self._urls.append(url.strip().format(server.port))

        self.logger.info("areweslimyet run by %d pages, %d iterations, %d perTabPause, %d settleWaitTime"
                         % (self._pages_to_load, self._iterations, self._perTabPause, self._settleWaitTime))
        self.logger.info("done setting up!")

    def tearDown(self):
        self.logger.info("tearing down!")

        self.logger.info("tearing down webservers!")
        self._webservers.stop()

        AwsyTestCase.tearDown(self)

        self.logger.info("done tearing down!")

    def clear_preloaded_browser(self):
        """
        Clears out the preloaded browser.

        Note: Does nothing on older builds that don't have a
              `gBrowser.removePreloadedBrowser` method.
        """
        self.logger.info("closing preloaded browser")
        script = """
            if ("removePreloadedBrowser" in gBrowser) {
                return gBrowser.removePreloadedBrowser();
            } else {
                return "gBrowser.removePreloadedBrowser not available";
            }
            """
        try:
            result = self.marionette.execute_script(script,
                                                    script_timeout=180000)
        except JavascriptException, e:
            self.logger.error("gBrowser.removePreloadedBrowser() JavaScript error: %s" % e)
        except ScriptTimeoutException:
            self.logger.error("gBrowser.removePreloadedBrowser() timed out")
        except:
            self.logger.error("gBrowser.removePreloadedBrowser() Unexpected error: %s" % sys.exc_info()[0])
        else:
            if result:
              self.logger.info(result)

    def test_open_tabs(self):
        """Marionette test entry that returns an array of checkoint arrays.

        This will generate a set of checkpoints for each iteration requested.
        Upon succesful completion the results will be stored in
        |self.testvars["results"]| and accessible to the test runner via the
        |testvars| object it passed in.
        """
        # setup the results array
        results = [[] for _ in range(self.iterations())]

        def create_checkpoint(name, iteration):
            checkpoint = self.do_memory_report(name, iteration)
            self.assertIsNotNone(checkpoint, "Checkpoint was recorded")
            results[iteration].append(checkpoint)

        # The first iteration gets Start and StartSettled entries before
        # opening tabs
        create_checkpoint("Start", 0)
        self.settle()
        create_checkpoint("StartSettled", 0)

        for itr in range(self.iterations()):
            self.open_pages()

            create_checkpoint("TabsOpen", itr)
            self.settle()
            create_checkpoint("TabsOpenSettled", itr)
            self.assertTrue(self.do_full_gc())
            create_checkpoint("TabsOpenForceGC", itr)

            # Close all tabs
            self.reset_state()

            with self.marionette.using_context('content'):
                self.logger.info("navigating to about:blank")
                self.marionette.navigate("about:blank")
                self.logger.info("navigated to about:blank")
            self.signal_user_active()

            # Create checkpoint that may contain retained processes that will
            # be reused.
            create_checkpoint("TabsClosedExtraProcesses", itr)

            # Clear out the retained processes and measure again.
            self.clear_preloaded_browser()

            create_checkpoint("TabsClosed", itr)
            self.settle()
            create_checkpoint("TabsClosedSettled", itr)
            self.assertTrue(self.do_full_gc(), "GC ran")
            create_checkpoint("TabsClosedForceGC", itr)

        # TODO(ER): Temporary hack until bug 1121139 lands
        self.logger.info("setting results")
        self.testvars["results"] = results
