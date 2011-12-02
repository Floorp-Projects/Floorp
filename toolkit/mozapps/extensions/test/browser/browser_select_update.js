/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the update part of the post-app-update dialog

var gProvider;
var gWin;

function waitForView(aView, aCallback) {
  var view = gWin.document.getElementById(aView);
  view.addEventListener("ViewChanged", function() {
    view.removeEventListener("ViewChanged", arguments.callee, false);
    aCallback();
  }, false);
}

function waitForClose(aCallback) {
  gWin.addEventListener("unload", function() {
    gWin.removeEventListener("unload", arguments.callee, false);

    aCallback();
  }, false);
}

/**
 * Creates 4 test add-ons. Two are disabled and two enabled.
 */
function setupUI(aFailDownloads, aFailInstalls, aCallback) {
  if (gProvider)
    gProvider.unregister();

  gProvider = new MockProvider();

  for (var i = 1; i < 5; i++) {
    var addon = new MockAddon("test" + i + "@tests.mozilla.org",
                              "Test Add-on " + i, "extension");
    addon.version = "1.0";
    addon.userDisabled = (i > 2);
    addon.appDisabled = false;
    addon.isActive = !addon.userDisabled && !addon.appDisabled;

    addon.findUpdates = function(aListener, aReason, aAppVersion, aPlatformVersion) {
      var newAddon = new MockAddon(this.id, this.name, "extension");
      newAddon.version = "2.0";
      var install = new MockInstall(this.name, this.type, newAddon);
      install.existingAddon = this;

      install.install = function() {
        this.state = AddonManager.STATE_DOWNLOADING;
        this.callListeners("onDownloadStarted");

        var self = this;
        executeSoon(function() {
          if (aFailDownloads) {
            self.state = AddonManager.STATE_DOWNLOAD_FAILED;
            self.callListeners("onDownloadFailed");
            return;
          }

          self.type = self._type;
          self.addon = new MockAddon(self.existingAddon.id, self.name, self.type);
          self.addon.version = self.version;
          self.addon.pendingOperations = AddonManager.PENDING_INSTALL;
          self.addon.install = self;

          self.existingAddon.pendingUpgrade = self.addon;
          self.existingAddon.pendingOperations |= AddonManager.PENDING_UPGRADE;

          self.state = AddonManager.STATE_DOWNLOADED;
          self.callListeners("onDownloadEnded");

          self.state = AddonManager.STATE_INSTALLING;
          self.callListeners("onInstallStarted");

          if (aFailInstalls) {
            self.state = AddonManager.STATE_INSTALL_FAILED;
            self.callListeners("onInstallFailed");
            return;
          }

          self.state = AddonManager.STATE_INSTALLED;
          self.callListeners("onInstallEnded");
        });
      }

      aListener.onUpdateAvailable(this, install);

      aListener.onUpdateFinished(this, AddonManager.UPDATE_STATUS_NO_ERROR);
    };

    gProvider.addAddon(addon);
  }

  gWin = Services.ww.openWindow(null,
                                "chrome://mozapps/content/extensions/selectAddons.xul",
                                "",
                                "chrome,centerscreen,dialog,titlebar",
                                null);
  waitForFocus(function() {
    waitForView("select", function() {
      var row = gWin.document.getElementById("select-rows").firstChild.nextSibling;
      while (row) {
        if (row.id == "test2@tests.mozilla.org" ||
            row.id == "test4@tests.mozilla.org") {
          row.disable();
        }
        else {
          row.keep();
        }
        row = row.nextSibling;
      }

      waitForView("confirm", function() {
        waitForView("update", aCallback);
        EventUtils.synthesizeMouseAtCenter(gWin.document.getElementById("next"), {}, gWin);
      });
      EventUtils.synthesizeMouseAtCenter(gWin.document.getElementById("next"), {}, gWin);
    });
  }, gWin);
}

function test() {
  waitForExplicitFinish();
  run_next_test();
}

function end_test() {
  finish();
}

// Test for working updates
add_test(function working_test() {
  setupUI(false, false, function() {
    waitForClose(function() {
      is(gWin.document.getElementById("update-progress").value, 2, "Should have finished 2 downloads");
      run_next_test();
    });

    EventUtils.synthesizeMouseAtCenter(gWin.document.getElementById("next"), {}, gWin);
  });
});

// Test for failed updates
add_test(function working_test() {
  setupUI(true, false, function() {
    waitForView("errors", function() {
      is(gWin.document.getElementById("update-progress").value, 2, "Should have finished 2 downloads");
      gWin.close();

      run_next_test();
    });

    EventUtils.synthesizeMouseAtCenter(gWin.document.getElementById("next"), {}, gWin);
  });
});

// Test for failed updates
add_test(function working_test() {
  setupUI(false, true, function() {
    waitForView("errors", function() {
      is(gWin.document.getElementById("update-progress").value, 2, "Should have finished 2 downloads");
      gWin.close();

      run_next_test();
    });

    EventUtils.synthesizeMouseAtCenter(gWin.document.getElementById("next"), {}, gWin);
  });
});
