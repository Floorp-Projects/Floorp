/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that certain types of addons do not have their version number
// displayed. This currently only includes lightweight themes.

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

// This test is testing XUL about:addons UI (and the HTML about:addons
// actually shows the version also on themes).
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

add_task(async function test() {
  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "extension@tests.mozilla.org",
    name: "Extension 1",
    type: "extension",
    version: "123",
  }, {
    id: "theme@tests.mozilla.org",
    name: "Theme 2",
    type: "theme",
    version: "456",
  }, {
    id: "lwtheme@personas.mozilla.org",
    name: "Persona 3",
    type: "theme",
    version: "789",
  }]);

  gManagerWindow = await open_manager();
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
});

function get(aId) {
  return gManagerWindow.document.getElementById(aId);
}

function get_node(parent, anonid) {
  return parent.ownerDocument.getAnonymousElementByAttribute(parent, "anonid", anonid);
}

function open_details(aList, aItem, aCallback) {
  aList.ensureElementIsVisible(aItem);
  EventUtils.synthesizeMouseAtCenter(aItem, {}, gManagerWindow);
  return new Promise(resolve => wait_for_view_load(gManagerWindow, resolve));
}

var check_addon_has_version = async function(aList, aName, aVersion) {
  for (let i = 0; i < aList.itemCount; i++) {
    let item = aList.getItemAtIndex(i);
    if (get_node(item, "name").textContent === aName) {
      ok(true, "Item with correct name found");
      let { version } = await get_tooltip_info(item);
      is(version, aVersion, "Item has correct version");
      return item;
    }
  }
  ok(false, "Item with correct name was not found");
  return null;
};

add_task(async function() {
  await gCategoryUtilities.openType("extension");
  info("Extension");
  let list = gManagerWindow.document.getElementById("addon-list");
  let item = await check_addon_has_version(list, "Extension 1", "123");
  await open_details(list, item);
  is_element_visible(get("detail-version"), "Details view has version visible");
  is(get("detail-version").value, "123", "Details view has correct version");
});

add_task(async function() {
  await gCategoryUtilities.openType("theme");
  info("Normal theme");
  let list = gManagerWindow.document.getElementById("addon-list");
  let item = await check_addon_has_version(list, "Theme 2", "456");
  await open_details(list, item);
  is_element_visible(get("detail-version"), "Details view has version visible");
  is(get("detail-version").value, "456", "Details view has correct version");
});

add_task(async function() {
  await gCategoryUtilities.openType("theme");
  info("Lightweight theme");
  let list = gManagerWindow.document.getElementById("addon-list");
  // See that the version isn't displayed
  let item = await check_addon_has_version(list, "Persona 3", undefined);
  await open_details(list, item);
  is_element_hidden(get("detail-version"), "Details view has version hidden");
  // If the version element is hidden then we don't care about its value
});

add_task(function end_test() {
  close_manager(gManagerWindow, finish);
});
