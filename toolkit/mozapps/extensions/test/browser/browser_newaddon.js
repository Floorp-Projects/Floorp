/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the new add-on tab

var gProvider;

function loadPage(aURL, aCallback, aBackground = false) {
  let tab = gBrowser.addTab();
  if (!aBackground)
    gBrowser.selectedTab = tab;
  let browser = tab.linkedBrowser;
  browser.loadURI(aURL);
  browser.addEventListener("AddonDisplayed", function(event) {
    browser.removeEventListener("AddonDisplayed", arguments.callee, false);

    aCallback(tab);
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
    seen: false,
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test 2",
    version: "7.1",
    creator: "Dave Townsend",
    userDisabled: true,
    seen: false
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

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      ok(aAddon.seen, "Add-on should have been marked as seen");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      is(gBrowser.tabs.length, 1, "Page should have been closed");

      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      aAddon.seen = false;
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

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      ok(aAddon.seen, "Add-on should have been marked as seen");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("allow"),
                                         {}, aTab.linkedBrowser.contentWindow);

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      is(gBrowser.tabs.length, 1, "Page should have been closed");

      ok(!aAddon.userDisabled, "Add-on should now have been enabled");

      ok(aAddon.isActive, "Add-on should now be running");

      aAddon.userDisabled = true;
      aAddon.seen = false;
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

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(aAddon) {
      ok(aAddon.seen, "Add-on should have been marked as seen");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      is(gBrowser.tabs.length, 1, "Page should have been closed");

      ok(aAddon.userDisabled, "Add-on should not have been enabled");

      ok(!aAddon.isActive, "Add-on should not be running");

      aAddon.seen = false;
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

    AddonManager.getAddonByID("addon2@tests.mozilla.org", function(aAddon) {
      ok(aAddon.seen, "Add-on should have been marked as seen");

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("allow"),
                                         {}, aTab.linkedBrowser.contentWindow);

      EventUtils.synthesizeMouseAtCenter(doc.getElementById("continue-button"),
                                         {}, aTab.linkedBrowser.contentWindow);

      is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("restartPanel"),
         "Should be showing the right buttons");

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

      aAddon.seen = false;
      run_next_test();
    });
  });
});

// Tests that opening the page in the background doesn't mark as seen
add_test(function() {
  loadPage("about:newaddon?id=addon1@tests.mozilla.org", function(aTab) {
    var doc = aTab.linkedBrowser.contentDocument;
    is(doc.getElementById("name").value, "Test 1 5.3", "Should say the right name");

    is_element_hidden(doc.getElementById("author"), "Should be no author displayed");
    is_element_hidden(doc.getElementById("location"), "Should be no location displayed");

    is(doc.getElementById("buttonDeck").selectedPanel, doc.getElementById("continuePanel"),
       "Should be showing the right buttons");

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(aAddon) {
      ok(!aAddon.seen, "Add-on should not have been marked as seen.");

      gBrowser.selectedTab = aTab;

      waitForFocus(function() {
        ok(aAddon.seen, "Add-on should have been marked as seen after focusing the tab.");

        gBrowser.removeTab(aTab);

        run_next_test();
      }, aTab.linkedBrowser.contentWindow);
    });
  }, true);
});
