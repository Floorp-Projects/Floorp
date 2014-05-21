/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the locale category is shown if there are no locale packs
// installed but some are pending install

var gManagerWindow;
var gCategoryUtilities;
var gProvider;
var gInstallProperties = [{
  name: "Locale Category Test",
  type: "locale"
}];
var gInstall;
var gExpectedCancel = false;
var gTestInstallListener = {
  onInstallStarted: function(aInstall) {
    check_hidden(false);
  },

  onInstallEnded: function(aInstall) {
    check_hidden(false);
    run_next_test();
  },

  onInstallCancelled: function(aInstall) {
    ok(gExpectedCancel, "Should expect install cancel");
    check_hidden(false);
    run_next_test();
  },

  onInstallFailed: function(aInstall) {
    ok(false, "Did not expect onInstallFailed");
    run_next_test();
  }
};

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

function check_hidden(aExpectedHidden) {
  var hidden = !gCategoryUtilities.isTypeVisible("locale");
  is(hidden, aExpectedHidden, "Should have correct hidden state");
}

// Tests that a non-active install does not make the locale category show
add_test(function() {
  check_hidden(true);
  gInstall = gProvider.createInstalls(gInstallProperties)[0];
  gInstall.addTestListener(gTestInstallListener);
  check_hidden(true);
  run_next_test();
});

// Test that restarting the add-on manager with a non-active install
// does not cause the locale category to show
add_test(function() {
  restart_manager(gManagerWindow, null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    check_hidden(true);
    run_next_test();
  });
});

// Test that installing the install shows the locale category
add_test(function() {
  gInstall.install();
});

// Test that restarting the add-on manager does not cause the locale category
// to become hidden
add_test(function() {
  restart_manager(gManagerWindow, null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    check_hidden(false);

    gExpectedCancel = true;
    gInstall.cancel();
  });
});

