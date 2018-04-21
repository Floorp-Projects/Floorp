/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests that double-click does not go to detail view if the target is a link or button.
 */

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();

  var gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "test1@tests.mozilla.org",
    name: "Test add-on 1",
    description: "foo",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }]);

  run_next_test();
}

function end_test() {
  finish();
}

function is_in_list(aManager, view) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(get_current_view(aManager).id, "list-view", "Should be on the right view");
}

function is_in_detail(aManager, view) {
  var doc = aManager.document;

  is(doc.getElementById("categories").selectedItem.value, view, "Should be on the right category");
  is(get_current_view(aManager).id, "detail-view", "Should be on the right view");
}

// Check that double-click does something.
add_test(async function() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension");

  var addon = get_addon_element(aManager, "test1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 1 }, aManager);
  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 2 }, aManager);

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_detail(aManager, "addons://list/extension");

  close_manager(aManager, run_next_test);
});

// Check that double-click does nothing when over the disable button.
add_test(async function() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension");

  var addon = get_addon_element(aManager, "test1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(
    aManager.document.getAnonymousElementByAttribute(addon, "anonid", "disable-btn"),
    { clickCount: 1 },
    aManager
  );
  // The disable button is replaced by the enable button when clicked on.
  EventUtils.synthesizeMouseAtCenter(
    aManager.document.getAnonymousElementByAttribute(addon, "anonid", "enable-btn"),
    { clickCount: 2 },
    aManager
  );

  aManager = await wait_for_view_load(aManager);
  info("Part 2");
  is_in_list(aManager, "addons://list/extension");

  close_manager(aManager, run_next_test);
});

// Check that double-click does nothing when over the undo button.
add_test(async function() {
  let aManager = await open_manager("addons://list/extension");
  info("Part 1");
  is_in_list(aManager, "addons://list/extension");

  var addon = get_addon_element(aManager, "test1@tests.mozilla.org");
  addon.parentNode.ensureElementIsVisible(addon);
  EventUtils.synthesizeMouseAtCenter(
    aManager.document.getAnonymousElementByAttribute(addon, "anonid", "remove-btn"),
    { clickCount: 1 },
    aManager
  );

  // The undo button is removed when clicked on.
  // We need to wait for the UI to catch up.
  setTimeout(async function() {
    var target = aManager.document.getAnonymousElementByAttribute(addon, "anonid", "undo-btn");
    var rect = target.getBoundingClientRect();
    var addonRect = addon.getBoundingClientRect();

    EventUtils.synthesizeMouse(target, rect.width / 2, rect.height / 2, { clickCount: 1 }, aManager);
    EventUtils.synthesizeMouse(addon,
      rect.left - addonRect.left + rect.width / 2,
      rect.top - addonRect.top + rect.height / 2,
      { clickCount: 2 },
      aManager
    );

    aManager = await wait_for_view_load(aManager);
    info("Part 2");
    is_in_list(aManager, "addons://list/extension");

    close_manager(aManager, run_next_test);
  }, 0);
});
