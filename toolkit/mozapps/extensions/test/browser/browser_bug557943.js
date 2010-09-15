/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 557943 - Searching for addons can result in wrong results

var gManagerWindow;

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Microsoft .NET Framework Assistant",
    description: "",
    version: "6.66"
  }, {
    id: "addon2@tests.mozilla.org",
    name: "AwesomeNet Addon",
    description: ""
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Dictionnaire MySpell en Francais (réforme 1990)",
    description: ""
  }]);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}


function perform_search(aQuery, aCallback) {
  var searchBox = gManagerWindow.document.getElementById("header-search");
  searchBox.value = aQuery;

  EventUtils.synthesizeMouse(searchBox, 2, 2, { }, gManagerWindow);
  EventUtils.synthesizeKey("VK_RETURN", { }, gManagerWindow);
  wait_for_view_load(gManagerWindow, function() {
    var list = gManagerWindow.document.getElementById("search-list");
    var rows = list.getElementsByTagName("richlistitem");
    aCallback(rows);
  });
}


add_test(function() {
  perform_search(".net", function(aRows) {
    is(aRows.length, 1, "Should only get one result");
    is(aRows[0].mAddon.id, "addon1@tests.mozilla.org", "Should get expected addon as only result");
    run_next_test();
  });
});

add_test(function() {
  perform_search("réf", function(aRows) {
    is(aRows.length, 1, "Should only get one result");
    is(aRows[0].mAddon.id, "addon3@tests.mozilla.org", "Should get expected addon as only result");
    run_next_test();
  });
});

add_test(function() {
  perform_search("javascript:void()", function(aRows) {
    is(aRows.length, 0, "Should not get any results");
    run_next_test();
  });
});
