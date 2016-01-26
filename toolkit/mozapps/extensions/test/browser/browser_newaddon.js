/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the new add-on tab

var gProvider;

function loadPage(aURL, aCallback) {
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(aURL);
  gBrowser.addEventListener("AddonDisplayed", function(event) {
    gBrowser.removeEventListener("AddonDisplayed", arguments.callee, false);

    aCallback(gBrowser.selectedTab);
  });
}

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test 1",
    version: "5.3",
    userDisabled: true,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test 2",
    version: "7.1",
    creator: "Dave Townsend",
    userDisabled: true
  }]);

  run_next_test();
}

function end_test() {
  finish();
}

// Tests that ignoring a restartless add-on works
add_test(function() {
  loadPage("about:newaddon?id=addon1@tests.mozilla.org", function(aTab) {
    var doc = aTab.linkedBrowser.contentDocument;
    is(doc.getElementById("name").value, "Test 1 5.3", "Should say the right name");

    is_element_hidden(doc.getElementById("author"), "Should be no author displayed");
    is_element_hidden(doc.getElementById("location"), "Should be no location displayed");

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
       "Should be showing the right buttons");

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                       {}, aTab.linkedBrowser.contentWindow);

    is(gBrowser.tabs.length, 1, "Page should have been closed");

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      run_next_test();
    });
  });
});

// Tests that enabling a restartless add-on works
add_test(function() {
  loadPage("about:newaddon?id=addon1@tests.mozilla.org", function(aTab) {
    var doc = aTab.linkedBrowser.contentDocument;
    is(doc.getElementById("name").value, "Test 1 5.3", "Should say the right name");

    is_element_hidden(doc.getElementById("author"), "Should be no author displayed");
    is_element_hidden(doc.getElementById("location"), "Should be no location displayed");

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
       "Should be showing the right buttons");

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("allow"),
                                       {}, aTab.linkedBrowser.contentWindow);

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                       {}, aTab.linkedBrowser.contentWindow);

    is(gBrowser.tabs.length, 1, "Page should have been closed");

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      ok(!aAddon.userDisabled, "Add-on should now have been enabled");

      ok(aAddon.isActive, "Add-on should now be running");

      run_next_test();
    });
  });
});

// Tests that ignoring a non-restartless add-on works
add_test(function() {
  loadPage("about:newaddon?id=addon2@tests.mozilla.org", function(aTab) {
    var doc = aTab.linkedBrowser.contentDocument;
    is(doc.getElementById("name").value, "Test 2 7.1", "Should say the right name");

    is_element_visible(doc.getElementById("author"), "Should be an author displayed");
    is(doc.getElementById("author").value, "By Dave Townsend", "Should have the right author");
    is_element_hidden(doc.getElementById("location"), "Should be no location displayed");

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
       "Should be showing the right buttons");

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                       {}, aTab.linkedBrowser.contentWindow);

    is(gBrowser.tabs.length, 1, "Page should have been closed");

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(aAddon) {
      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      run_next_test();
    });
  });
});

// Tests that enabling a non-restartless add-on works
add_test(function() {
  loadPage("about:newaddon?id=addon2@tests.mozilla.org", function(aTab) {
    var doc = aTab.linkedBrowser.contentDocument;
    is(doc.getElementById("name").value, "Test 2 7.1", "Should say the right name");

    is_element_visible(doc.getElementById("author"), "Should be an author displayed");
    is(doc.getElementById("author").value, "By Dave Townsend", "Should have the right author");
    is_element_hidden(doc.getElementById("location"), "Should be no location displayed");

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
       "Should be showing the right buttons");

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("allow"),
                                       {}, aTab.linkedBrowser.contentWindow);

    EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                       {}, aTab.linkedBrowser.contentWindow);

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("restartPanel"),
       "Should be showing the right buttons");

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(aAddon) {
      ok(!aAddon.userDisabled, "Add-on should now have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      ok(doc.getElementById("allow").disabled, "Should have disabled checkbox");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("cancel-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
         "Should be showing the right buttons");

      ok(!doc.getElementById("allow").disabled, "Should have enabled checkbox");

      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("allow"),
                                         {}, aTab.linkedBrowser.contentWindow);

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      is(gBrowser.tabs.length, 1, "Page should have been closed");

      run_next_test();
    });
  });
});
