# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import fnmatch
import glob
import gzip
import json
import os
import shutil
import sys
import tempfile
import time

import mozlog.structured

from marionette_driver import Wait
from marionette_driver.legacy_actions import Actions
from marionette_driver.errors import JavascriptException, ScriptTimeoutException
from marionette_driver.keys import Keys
from marionette_harness import MarionetteTestCase

AWSY_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
if AWSY_PATH not in sys.path:
    sys.path.append(AWSY_PATH)

from awsy import ITERATIONS, PER_TAB_PAUSE, SETTLE_WAIT_TIME, MAX_TABS
from awsy import process_perf_data


class AwsyTestCase(MarionetteTestCase):
    """
    Base test case for AWSY tests.
    """

    def urls(self):
        raise NotImplementedError()

    def perf_suites(self):
        raise NotImplementedError()

    def perf_checkpoints(self):
        raise NotImplementedError()

    def perf_extra_opts(self):
        return None

    def iterations(self):
        return self._iterations

    def pages_to_load(self):
        return self._pages_to_load if self._pages_to_load else len(self.urls())

    def settle(self):
        """
        Pauses for the settle time.
        """
        time.sleep(self._settleWaitTime)

    def setUp(self):
        MarionetteTestCase.setUp(self)

        self.logger = mozlog.structured.structuredlog.get_default_logger()
        self.marionette.set_context('chrome')
        self._resultsDir = self.testvars["resultsDir"]

        self._binary = self.testvars['bin']
        self._run_local = self.testvars.get('run_local', False)

        # Cleanup our files from previous runs.
        for patt in ('memory-report-*.json.gz',
                     'perfherder_data.json',
                     'dmd-*.json.gz'):
            for f in glob.glob(os.path.join(self._resultsDir, patt)):
                os.unlink(f)

        # Optional testvars.
        self._pages_to_load = self.testvars.get("entities", 0)
        self._iterations = self.testvars.get("iterations", ITERATIONS)
        self._perTabPause = self.testvars.get("perTabPause", PER_TAB_PAUSE)
        self._settleWaitTime = self.testvars.get("settleWaitTime", SETTLE_WAIT_TIME)
        self._maxTabs = self.testvars.get("maxTabs", MAX_TABS)
        self._dmd = self.testvars.get("dmd", False)

        self.logger.info("areweslimyet run by %d pages, %d iterations,"
                         " %d perTabPause, %d settleWaitTime"
                         % (self._pages_to_load, self._iterations,
                            self._perTabPause, self._settleWaitTime))
        self.reset_state()

    def tearDown(self):
        MarionetteTestCase.tearDown(self)

        try:
            self.logger.info("processing data in %s!" % self._resultsDir)
            perf_blob = process_perf_data.create_perf_data(
                            self._resultsDir, self.perf_suites(),
                            self.perf_checkpoints(),
                            self.perf_extra_opts())
            self.logger.info("PERFHERDER_DATA: %s" % json.dumps(perf_blob))

            perf_file = os.path.join(self._resultsDir, "perfherder_data.json")
            with open(perf_file, 'w') as fp:
                json.dump(perf_blob, fp, indent=2)
            self.logger.info("Perfherder data written to %s" % perf_file)
        except Exception:
            raise
        finally:
            # Make sure we cleanup and upload any existing files even if there
            # were errors processing the perf data.
            if self._dmd:
                self.cleanup_dmd()

            # copy it to moz upload dir if set
            if 'MOZ_UPLOAD_DIR' in os.environ:
                for file in os.listdir(self._resultsDir):
                    file = os.path.join(self._resultsDir, file)
                    if os.path.isfile(file):
                        shutil.copy2(file, os.environ["MOZ_UPLOAD_DIR"])

    def cleanup_dmd(self):
        """
        Handles moving DMD reports from the temp dir to our resultsDir.
        """
        from dmd import fixStackTraces

        # Move DMD files from temp dir to resultsDir.
        tmpdir = tempfile.gettempdir()
        tmp_files = os.listdir(tmpdir)
        for f in fnmatch.filter(tmp_files, "dmd-*.json.gz"):
            f = os.path.join(tmpdir, f)
            # We don't fix stacks on Windows, even though we could, due to the
            # tale of woe in bug 1626272.
            if not sys.platform.startswith('win'):
                self.logger.info("Fixing stacks for %s, this may take a while" % f)
                isZipped = True
                fixStackTraces(f, isZipped, gzip.open)
            shutil.move(f, self._resultsDir)

        # Also attempt to cleanup the unified memory reports.
        for f in fnmatch.filter(tmp_files, "unified-memory-report-*.json.gz"):
            try:
                os.remove(f)
            except OSError:
                self.logger.info("Unable to remove %s" % f)

    def reset_state(self):
        self._pages_loaded = 0

        # Close all tabs except one
        for x in self.marionette.window_handles[1:]:
            self.logger.info("closing window: %s" % x)
            self.marionette.switch_to_window(x)
            self.marionette.close()

        self._tabs = self.marionette.window_handles
        self.marionette.switch_to_window(self._tabs[0])

    def do_full_gc(self):
        """Performs a full garbage collection cycle and returns when it is finished.

        Returns True on success and False on failure.
        """
        # NB: we could do this w/ a signal or the fifo queue too
        self.logger.info("starting gc...")
        gc_script = """
            let [resolve] = arguments;
            Cu.import("resource://gre/modules/Services.jsm");
            Services.obs.notifyObservers(null, "child-mmu-request", null);

            let memMgrSvc =
            Cc["@mozilla.org/memory-reporter-manager;1"].getService(
            Ci.nsIMemoryReporterManager);
            memMgrSvc.minimizeMemoryUsage(() => {resolve("gc done!");});
            """
        result = None
        try:
            result = self.marionette.execute_async_script(
                gc_script, script_timeout=180000)
        except JavascriptException as e:
            self.logger.error("GC JavaScript error: %s" % e)
        except ScriptTimeoutException:
            self.logger.error("GC timed out")
        except Exception:
            self.logger.error("Unexpected error: %s" % sys.exc_info()[0])
        else:
            self.logger.info(result)

        return result is not None

    def do_memory_report(self, checkpointName, iteration):
        """Creates a memory report for all processes and and returns the
        checkpoint.

        This will block until all reports are retrieved or a timeout occurs.
        Returns the checkpoint or None on error.

        :param checkpointName: The name of the checkpoint.
        """
        self.logger.info("starting checkpoint %s..." % checkpointName)

        checkpoint_file = "memory-report-%s-%d.json.gz" % (checkpointName, iteration)
        checkpoint_path = os.path.join(self._resultsDir, checkpoint_file)
        # On Windows, replace / with the Windows directory
        # separator \ and escape it to prevent it from being
        # interpreted as an escape character.
        if sys.platform.startswith('win'):
            checkpoint_path = (checkpoint_path.
                               replace('\\', '\\\\').
                               replace('/', '\\\\'))

        checkpoint_script = r"""
            let [resolve] = arguments;
            let dumper =
            Cc["@mozilla.org/memory-info-dumper;1"].getService(
            Ci.nsIMemoryInfoDumper);
            dumper.dumpMemoryReportsToNamedFile(
                "%s",
                () => resolve("memory report done!"),
                null,
                /* anonymize */ false);
            """ % checkpoint_path

        checkpoint = None
        try:
            finished = self.marionette.execute_async_script(
                checkpoint_script, script_timeout=60000)
            if finished:
                checkpoint = checkpoint_path
        except JavascriptException as e:
            self.logger.error("Checkpoint JavaScript error: %s" % e)
        except ScriptTimeoutException:
            self.logger.error("Memory report timed out")
        except Exception:
            self.logger.error("Unexpected error: %s" % sys.exc_info()[0])
        else:
            self.logger.info("checkpoint created, stored in %s" % checkpoint_path)

        # Now trigger a DMD report if requested.
        if self._dmd:
            self.do_dmd(checkpointName, iteration)

        return checkpoint

    def do_dmd(self, checkpointName, iteration):
        """
        Triggers DMD reports that are used to help identify sources of
        'heap-unclassified'.

        NB: This will dump DMD reports to the temp dir. Unfortunately it also
        dumps memory reports, but that's all we have to work with right now.
        """
        self.logger.info("Starting %s DMD reports..." % checkpointName)

        ident = "%s-%d" % (checkpointName, iteration)

        # TODO(ER): This actually takes a minimize argument. We could use that
        # rather than have a separate `do_gc` function. Also it generates a
        # memory report so we could combine this with `do_checkpoint`. The main
        # issue would be moving everything out of the temp dir.
        #
        # Generated files have the form:
        #   dmd-<checkpoint>-<iteration>-pid.json.gz, ie:
        #   dmd-TabsOpenForceGC-0-10885.json.gz
        #
        # and for the memory report:
        #   unified-memory-report-<checkpoint>-<iteration>.json.gz
        dmd_script = r"""
            let dumper =
            Cc["@mozilla.org/memory-info-dumper;1"].getService(
            Ci.nsIMemoryInfoDumper);
            dumper.dumpMemoryInfoToTempDir(
                "%s",
                /* anonymize = */ false,
                /* minimize = */ false);
            """ % ident

        try:
            # This is async and there's no callback so we use the existence
            # of an incomplete memory report to check if it hasn't finished yet.
            self.marionette.execute_script(dmd_script, script_timeout=60000)
            tmpdir = tempfile.gettempdir()
            prefix = "incomplete-unified-memory-report-%s-%d-*" % (checkpointName, iteration)
            max_wait = 240
            elapsed = 0
            while fnmatch.filter(os.listdir(tmpdir), prefix) and elapsed < max_wait:
                self.logger.info("Waiting for memory report to finish")
                time.sleep(1)
                elapsed += 1

            incomplete = fnmatch.filter(os.listdir(tmpdir), prefix)
            if incomplete:
                # The memory reports never finished.
                self.logger.error("Incomplete memory reports leftover.")
                for f in incomplete:
                    os.remove(os.path.join(tmpdir, f))

        except JavascriptException as e:
            self.logger.error("DMD JavaScript error: %s" % e)
        except ScriptTimeoutException:
            self.logger.error("DMD timed out")
        except Exception:
            self.logger.error("Unexpected error: %s" % sys.exc_info()[0])
        else:
            self.logger.info("DMD started, prefixed with %s" % ident)

    def open_and_focus(self):
        """Opens the next URL in the list and focuses on the tab it is opened in.

        A new tab will be opened if |_maxTabs| has not been exceeded, otherwise
        the URL will be loaded in the next tab.
        """
        page_to_load = self.urls()[self._pages_loaded % len(self.urls())]
        tabs_loaded = len(self._tabs)
        open_tab_script = r"""
            gBrowser.loadOneTab("about:blank", {
                inBackground: false,
                triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
            });
        """

        if tabs_loaded < self._maxTabs and tabs_loaded <= self._pages_loaded:
            full_tab_list = self.marionette.window_handles

            self.marionette.execute_script(open_tab_script, script_timeout=60000)

            Wait(self.marionette).until(
                lambda mn: len(mn.window_handles) == tabs_loaded + 1,
                message="No new tab has been opened"
            )

            # NB: The tab list isn't sorted, so we do a set diff to determine
            #     which is the new tab
            new_tab_list = self.marionette.window_handles
            new_tabs = list(set(new_tab_list) - set(full_tab_list))

            self._tabs.append(new_tabs[0])
            tabs_loaded += 1

        tab_idx = self._pages_loaded % self._maxTabs

        tab = self._tabs[tab_idx]

        # Tell marionette which tab we're on
        # NB: As a work-around for an e10s marionette bug, only select the tab
        #     if we're really switching tabs.
        if tabs_loaded > 1:
            self.logger.info("switching to tab")
            self.marionette.switch_to_window(tab)
            self.logger.info("switched to tab")

        with self.marionette.using_context('content'):
            self.logger.info("loading %s" % page_to_load)
            self.marionette.navigate(page_to_load)
            self.logger.info("loaded!")

        # The tab handle can change after actually loading content
        # First build a set up w/o the current tab
        old_tabs = set(self._tabs)
        old_tabs.remove(tab)
        # Perform a set diff to get the (possibly) new handle
        new_tabs = set(self.marionette.window_handles) - old_tabs
        # Update the tab list at the current index to preserve the tab
        # ordering
        if new_tabs:
            self._tabs[tab_idx] = list(new_tabs)[0]

        # give the page time to settle
        time.sleep(self._perTabPause)

        self._pages_loaded += 1

    def signal_user_active(self):
        """Signal to the browser that the user is active.

        Normally when being driven by marionette the browser thinks the
        user is inactive the whole time because user activity is
        detected by looking at key and mouse events.

        This would be a problem for this test because user inactivity is
        used to schedule some GCs (in particular shrinking GCs), so it
        would make this unrepresentative of real use.

        Instead we manually cause some inconsequential activity (a press
        and release of the shift key) to make the browser think the user
        is active.  Then when we sleep to allow things to settle the
        browser will see the user as becoming inactive and trigger
        appropriate GCs, as would have happened in real use.
        """
        action = Actions(self.marionette)
        action.key_down(Keys.SHIFT)
        action.key_up(Keys.SHIFT)
        action.perform()

    def open_pages(self):
        """
        Opens all pages with our given configuration.
        """
        for _ in range(self.pages_to_load()):
            self.open_and_focus()
            self.signal_user_active()
