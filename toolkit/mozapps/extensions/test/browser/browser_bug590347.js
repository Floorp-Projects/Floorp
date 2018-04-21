/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 590347
// Tests if softblock notifications are exposed in preference to incompatible
// notifications when compatibility checking is disabled

var gProvider;
var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;

// Opens the details view of an add-on
async function open_details(aId, aType, aCallback) {
  requestLongerTimeout(2);

  await gCategoryUtilities.openType(aType);
  var list = gManagerWindow.document.getElementById("addon-list");
  var item = list.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      list.ensureElementIsVisible(item);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, aCallback);
      return;
    }
    item = item.nextSibling;
  }
  ok(false, "Should have found the add-on in the list");
}

function get_list_view_warning_node() {
  let item = gManagerWindow.document.getElementById("addon-list").firstChild;
  let found = false;
  while (item) {
    if (item.mAddon.name == "Test add-on") {
      found = true;
      break;
    }
    item = item.nextSibling;
  }
  ok(found, "Test add-on node should have been found.");
  return item.ownerDocument.getAnonymousElementByAttribute(item, "anonid", "warning");
}

function get_detail_view_warning_node(aManagerWindow) {
  if (aManagerWindow)
    return aManagerWindow.document.getElementById("detail-warning");
  return undefined;
}

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on",
    description: "A test add-on",
    isCompatible: false,
    blocklistState: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
  }]);

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

// Check with compatibility checking enabled
add_test(async function() {
  await gCategoryUtilities.openType("extension");
  Services.prefs.setBoolPref(PREF_CHECK_COMPATIBILITY, true);
  let warning_node = get_list_view_warning_node();
  is_element_visible(warning_node, "Warning message should be visible");
  is(warning_node.textContent, "Test add-on is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
  run_next_test();
});

add_test(function() {
  open_details("addon1@tests.mozilla.org", "extension", function() {
    let warning_node = get_detail_view_warning_node(gManagerWindow);
    is_element_visible(warning_node, "Warning message should be visible");
    is(warning_node.textContent, "Test add-on is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    Services.prefs.setBoolPref(PREF_CHECK_COMPATIBILITY, false);
    run_next_test();
  });
});

// Check with compatibility checking disabled
add_test(async function() {
  await gCategoryUtilities.openType("extension");
  let warning_node = get_list_view_warning_node();
  is_element_visible(warning_node, "Warning message should be visible");
  is(warning_node.textContent, "Test add-on is known to cause security or stability issues.", "Warning message should be correct");
  run_next_test();
});

add_test(function() {
  open_details("addon1@tests.mozilla.org", "extension", function() {
    let warning_node = get_detail_view_warning_node(gManagerWindow);
    is_element_visible(warning_node, "Warning message should be visible");
    is(warning_node.textContent, "Test add-on is known to cause security or stability issues.", "Warning message should be correct");
    run_next_test();
  });
});
