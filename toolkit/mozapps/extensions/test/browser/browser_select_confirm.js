/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the confirmation part of the post-app-update dialog

var gProvider;
var gWin;

function waitForView(aView, aCallback) {
  var view = gWin.document.getElementById(aView);
  view.addEventListener("ViewChanged", function() {
    view.removeEventListener("ViewChanged", arguments.callee, false);
    try {
      aCallback();
    }
    catch (e) {
      ok(false, e);
    }
  }, false);
}

/**
 * Creates 4 test add-ons. Two are disabled and two enabled.
 *
 * @param  aAppDisabled
 *         The appDisabled property for the test add-ons
 * @param  aUpdateAvailable
 *         True if the test add-ons should claim to have an update available
 */
function setupUI(aAppDisabled, aUpdateAvailable, aCallback) {
  if (gProvider)
    gProvider.unregister();

  gProvider = new MockProvider();

  for (var i = 1; i < 5; i++) {
    var addon = new MockAddon("test" + i + "@tests.mozilla.org",
                              "Test Add-on " + i, "extension");
    addon.version = "1.0";
    addon.userDisabled = (i > 2);
    addon.appDisabled = aAppDisabled;
    addon.isActive = !addon.userDisabled && !addon.appDisabled;

    addon.findUpdates = function(aListener, aReason, aAppVersion, aPlatformVersion) {
      if (aUpdateAvailable) {
        var newAddon = new MockAddon(this.id, this.name, "extension");
        newAddon.version = "2.0";
        var install = new MockInstall(this.name, this.type, newAddon);
        install.existingAddon = this;
        aListener.onUpdateAvailable(this, install);
      }

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

      waitForView("confirm", aCallback);
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

// Test for disabling
add_test(function disabling_test() {
  setupUI(false, false, function() {
    ok(gWin.document.getElementById("incompatible-list").hidden, "Incompatible list should be hidden");
    ok(gWin.document.getElementById("update-list").hidden, "Update list should be hidden");

    var list = gWin.document.getElementById("disable-list");
    ok(!list.hidden, "Disable list should be visible");
    is(list.childNodes.length, 2, "Should be one add-on getting disabled (plus the header)");
    is(list.childNodes[1].id, "test2@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[1].getAttribute("name"), "Test Add-on 2", "Should be the right add-on name");

    var list = gWin.document.getElementById("enable-list");
    ok(!list.hidden, "Enable list should be visible");
    is(list.childNodes.length, 2, "Should be one add-on getting disabled (plus the header)");
    is(list.childNodes[1].id, "test3@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[1].getAttribute("name"), "Test Add-on 3", "Should be the right add-on name");

    ok(gWin.document.getElementById("next").hidden, "Next button should be hidden");
    ok(!gWin.document.getElementById("done").hidden, "Done button should be visible");
    gWin.close();

    run_next_test();
  });
});

// Test for incompatible
add_test(function incompatible_test() {
  setupUI(true, false, function() {
    ok(gWin.document.getElementById("update-list").hidden, "Update list should be hidden");
    ok(gWin.document.getElementById("disable-list").hidden, "Disable list should be hidden");
    ok(gWin.document.getElementById("enable-list").hidden, "Enable list should be hidden");

    var list = gWin.document.getElementById("incompatible-list");
    ok(!list.hidden, "Incompatible list should be visible");
    is(list.childNodes.length, 3, "Should be two add-ons waiting to be compatible (plus the header)");
    is(list.childNodes[1].id, "test1@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[1].getAttribute("name"), "Test Add-on 1", "Should be the right add-on name");
    is(list.childNodes[2].id, "test3@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[2].getAttribute("name"), "Test Add-on 3", "Should be the right add-on name");

    ok(gWin.document.getElementById("next").hidden, "Next button should be hidden");
    ok(!gWin.document.getElementById("done").hidden, "Done button should be visible");
    gWin.close();

    run_next_test();
  });
});

// Test for updates
add_test(function update_test() {
  setupUI(false, true, function() {
    ok(gWin.document.getElementById("incompatible-list").hidden, "Incompatible list should be hidden");
    ok(gWin.document.getElementById("enable-list").hidden, "Enable list should be hidden");

    var list = gWin.document.getElementById("update-list");
    ok(!list.hidden, "Update list should be visible");
    is(list.childNodes.length, 3, "Should be two add-ons waiting to be updated (plus the header)");
    is(list.childNodes[1].id, "test1@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[1].getAttribute("name"), "Test Add-on 1", "Should be the right add-on name");
    is(list.childNodes[2].id, "test3@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[2].getAttribute("name"), "Test Add-on 3", "Should be the right add-on name");

    list = gWin.document.getElementById("disable-list");
    ok(!list.hidden, "Disable list should be visible");
    is(list.childNodes.length, 2, "Should be one add-on getting disabled (plus the header)");
    is(list.childNodes[1].id, "test2@tests.mozilla.org", "Should be the right add-on ID");
    is(list.childNodes[1].getAttribute("name"), "Test Add-on 2", "Should be the right add-on name");

    ok(!gWin.document.getElementById("next").hidden, "Next button should be visible");
    ok(gWin.document.getElementById("done").hidden, "Done button should be hidden");
    gWin.close();

    run_next_test();
  });
});
