# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

# This test is not written in a very modular way because update tests are generally not done with
# Marionette, so this is sort of a one-off. We can't successfully just load all the actual setup
# code that the normal update tests use, so this test basically just copies the bits that it needs.
# The reason that this is a Marionette test at all is that, even if we stub out the quit/restart
# call, we need no windows to be open to test the relevant functionality, but xpcshell doesn't do
# windows at all and mochitest has a test runner window that Firefox recognizes, but mustn't close
# during testing.

from __future__ import absolute_import

from marionette_driver import errors, Wait
from marionette_harness import MarionetteTestCase


class TestNoWindowUpdateRestart(MarionetteTestCase):
    def setUp(self):
        super(TestNoWindowUpdateRestart, self).setUp()

        self.marionette.delete_session()
        self.marionette.start_session({"moz:windowless": True})
        # See Bug 1777956
        window = self.marionette.window_handles[0]
        self.marionette.switch_to_window(window)

        # Every part of this test ought to run in the chrome context.
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)
        self.setUpBrowser()
        self.origDisabledForTesting = self.marionette.get_pref(
            "app.update.disabledForTesting"
        )
        self.resetUpdate()

    def setUpBrowser(self):
        self.origAppUpdateAuto = self.marionette.execute_async_script(
            """
            let [resolve] = arguments;

            (async () => {
                Services.prefs.setIntPref("app.update.download.attempts", 0);
                Services.prefs.setIntPref("app.update.download.maxAttempts", 0);
                Services.prefs.setBoolPref("app.update.staging.enabled", false);
                Services.prefs.setBoolPref("app.update.noWindowAutoRestart.enabled", true);
                Services.prefs.setIntPref("app.update.noWindowAutoRestart.delayMs", 1000);
                Services.prefs.clearUserPref("testing.no_window_update_restart.silent_restart_env");

                let { UpdateUtils } = ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm");
                let origAppUpdateAuto = await UpdateUtils.getAppUpdateAutoEnabled();
                await UpdateUtils.setAppUpdateAutoEnabled(true);

                // Prevent the update sync manager from thinking there are two instances running
                let exePath = Services.dirsvc.get("XREExeF", Ci.nsIFile);
                let dirProvider = {
                    getFile: function AGP_DP_getFile(aProp, aPersistent) {
                        aPersistent.value = false;
                        switch (aProp) {
                            case "XREExeF":
                                exePath.append("browser-test");
                                return exePath;
                        }
                        return null;
                    },
                    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
                };
                let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
                ds.QueryInterface(Ci.nsIProperties).undefine("XREExeF");
                ds.registerProvider(dirProvider);
                let gSyncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
                  Ci.nsIUpdateSyncManager
                );
                gSyncManager.resetLock();
                ds.unregisterProvider(dirProvider);

                return origAppUpdateAuto;
            })().then(resolve);
        """
        )

    def tearDown(self):
        self.tearDownBrowser()
        self.resetUpdate()

        # Reset context to the default.
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

        super(TestNoWindowUpdateRestart, self).tearDown()

    def tearDownBrowser(self):
        self.marionette.execute_async_script(
            """
            let [origAppUpdateAuto, origDisabledForTesting, resolve] = arguments;
            (async () => {
                Services.prefs.setBoolPref("app.update.disabledForTesting", origDisabledForTesting);
                Services.prefs.clearUserPref("app.update.download.attempts");
                Services.prefs.clearUserPref("app.update.download.maxAttempts");
                Services.prefs.clearUserPref("app.update.staging.enabled");
                Services.prefs.clearUserPref("app.update.noWindowAutoRestart.enabled");
                Services.prefs.clearUserPref("app.update.noWindowAutoRestart.delayMs");
                Services.prefs.clearUserPref("testing.no_window_update_restart.silent_restart_env");

                let { UpdateUtils } = ChromeUtils.import("resource://gre/modules/UpdateUtils.jsm");
                await UpdateUtils.setAppUpdateAutoEnabled(origAppUpdateAuto);
            })().then(resolve);
        """,
            script_args=(self.origAppUpdateAuto, self.origDisabledForTesting),
        )

    def test_update_on_last_window_close(self):
        # By preparing an update and closing all windows, we activate the No
        # Window Update Restart feature (see Bug 1720742) which causes Firefox
        # to restart to install updates.
        self.marionette.restart(
            callback=self.prepare_update_and_close_all_windows, in_app=True
        )

        # Firefox should come back without any windows (i.e. silently).
        with self.assertRaises(errors.TimeoutException):
            wait = Wait(
                self.marionette,
                ignored_exceptions=errors.NoSuchWindowException,
                timeout=5,
            )
            wait.until(lambda _: self.marionette.window_handles)

        # Reset the browser and active WebDriver session
        self.marionette.restart(in_app=True)
        self.marionette.delete_session()
        self.marionette.start_session()
        self.marionette.set_context(self.marionette.CONTEXT_CHROME)

        quit_flags_correct = self.marionette.get_pref(
            "testing.no_window_update_restart.silent_restart_env"
        )
        self.assertTrue(quit_flags_correct)

        # Normally, the update status file would have been removed at this point by Post Update
        # Processing. But restarting resets app.update.disabledForTesting, which causes that to be
        # skipped, allowing us to look at the update status file directly.
        update_status_path = self.marionette.execute_script(
            """
            let statusFile = FileUtils.getDir("UpdRootD", ["updates", "0"], true);
            statusFile.append("update.status");
            return statusFile.path;
        """
        )
        with open(update_status_path, "r") as f:
            # If Firefox was built with "--enable-unverified-updates" (or presumably if we tested
            # with an actual, signed update), the update should succeed. Otherwise, it will fail
            # with CERT_VERIFY_ERROR (error code 19). Unfortunately, there is no good way to tell
            # which of those situations we are in. Luckily, it doesn't matter, because we aren't
            # trying to test whether the update applied successfully, just whether the
            # "No Window Update Restart" feature works.
            self.assertIn(f.read().strip(), ["succeeded", "failed: 19"])

    def resetUpdate(self):
        self.marionette.execute_script(
            """
            let UM = Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager);
            UM.QueryInterface(Ci.nsIObserver).observe(null, "um-reload-update-data", "skip-files");

            let { UpdateListener } = ChromeUtils.import("resource://gre/modules/UpdateListener.jsm");
            UpdateListener.reset();

            let { AppMenuNotifications } = ChromeUtils.import("resource://gre/modules/AppMenuNotifications.jsm");
            AppMenuNotifications.removeNotification(/.*/);

            // Remove old update files so that they don't interfere with tests.
            let rootUpdateDir = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
            let updateDir = rootUpdateDir.clone();
            updateDir.append("updates");
            let patchDir = updateDir.clone();
            patchDir.append("0");

            let filesToRemove = [];
            let addFileToRemove = (dir, filename) => {
                let file = dir.clone();
                file.append(filename);
                filesToRemove.push(file);
            };

            addFileToRemove(rootUpdateDir, "active-update.xml");
            addFileToRemove(rootUpdateDir, "updates.xml");
            addFileToRemove(patchDir, "bt.result");
            addFileToRemove(patchDir, "update.status");
            addFileToRemove(patchDir, "update.version");
            addFileToRemove(patchDir, "update.mar");
            addFileToRemove(patchDir, "updater.ini");
            addFileToRemove(updateDir, "backup-update.log");
            addFileToRemove(updateDir, "last-update.log");
            addFileToRemove(patchDir, "update.log");

            for (const file of filesToRemove) {
                try {
                    if (file.exists()) {
                        file.remove(false);
                    }
                } catch (e) {
                    console.warn("Unable to remove file. Path: '" + file.path + "', Exception: " + e);
                }
            }
        """
        )

    def prepare_update_and_close_all_windows(self):
        self.marionette.execute_async_script(
            """
            let [updateURLString, resolve] = arguments;

            (async () => {
                let updateDownloadedPromise = new Promise(innerResolve => {
                    Services.obs.addObserver(function callback() {
                        Services.obs.removeObserver(callback, "update-downloaded");
                        innerResolve();
                    }, "update-downloaded");
                });

                // Set the update URL to the one that was passed in.
                let mockAppInfo = Object.create(Services.appinfo, {
                    updateURL: {
                        configurable: true,
                        enumerable: true,
                        writable: false,
                        value: updateURLString,
                    },
                });
                Services.appinfo = mockAppInfo;

                // We aren't going to flip this until after the URL is set because the test fails
                // if we hit the real update server.
                Services.prefs.setBoolPref("app.update.disabledForTesting", false);

                let aus = Cc["@mozilla.org/updates/update-service;1"]
                    .getService(Ci.nsIApplicationUpdateService);
                aus.checkForBackgroundUpdates();

                await updateDownloadedPromise;

                Services.obs.addObserver((aSubject, aTopic, aData) => {
                    let env = Cc["@mozilla.org/process/environment;1"].getService(
                        Ci.nsIEnvironment
                    );
                    let silent_restart = env.get("MOZ_APP_SILENT_START") == 1 && env.get("MOZ_APP_RESTART") == 1;
                    Services.prefs.setBoolPref("testing.no_window_update_restart.silent_restart_env", silent_restart);
                }, "quit-application-granted");

                for (const win of Services.wm.getEnumerator("navigator:browser")) {
                    win.close();
                }
            })().then(resolve);
        """,
            script_args=(self.marionette.absolute_url("update.xml"),),
        )
