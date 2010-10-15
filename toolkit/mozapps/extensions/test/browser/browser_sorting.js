/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that sorting of add-ons in the list views works correctly

var gManagerWindow;
var gProvider;

function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "test1@tests.mozilla.org",
    name: "Test add-on",
    description: "foo",
    updateDate: new Date(2010, 04, 02, 00, 00, 00),
    size: 1
  }, {
    id: "test2@tests.mozilla.org",
    name: "a first add-on",
    description: "foo",
    updateDate: new Date(2010, 04, 01, 23, 59, 59),
    size: 0265
  }, {
    id: "test3@tests.mozilla.org",
    name: "\u010Cesk\u00FD slovn\u00EDk", // Český slovník
    description: "foo",
    updateDate: new Date(2010, 04, 02, 00, 00, 01),
    size: 12
  }, {
    id: "test4@tests.mozilla.org",
    name: "canadian dictionary",
    updateDate: new Date(1970, 0, 01, 00, 00, 00),
    description: "foo",
  }, {
    id: "test5@tests.mozilla.org",
    name: "croatian dictionary",
    description: "foo",
    updateDate: new Date(2012, 12, 12, 00, 00, 00),
    size: 5
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

function check_order(aExpectedOrder) {
  var order = [];
  var list = gManagerWindow.document.getElementById("addon-list");
  var node = list.firstChild;
  while (node) {
    var id = node.getAttribute("value");
    if (id && id.substring(id.length - 18) != "@tests.mozilla.org")
      return;
    order.push(node.getAttribute("value"));
    node = node.nextSibling;
  }

  is(order.toSource(), aExpectedOrder.toSource(), "Should have seen the right order");
}

// Tests that ascending name ordering was the default
add_test(function() {
  check_order([
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test1@tests.mozilla.org"
  ]);

  run_next_test();
});

// Tests that switching to date ordering works and defaults to descending
add_test(function() {
  var sorters = gManagerWindow.document.getElementById("list-sorters");
  var nameSorter = gManagerWindow.document.getAnonymousElementByAttribute(sorters, "anonid", "date-btn");
  EventUtils.synthesizeMouseAtCenter(nameSorter, { }, gManagerWindow);

  check_order([
    "test5@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org"
  ]);

  run_next_test();
});

// Tests that switching to name ordering works and defaults to ascending
add_test(function() {
  var sorters = gManagerWindow.document.getElementById("list-sorters");
  var nameSorter = gManagerWindow.document.getAnonymousElementByAttribute(sorters, "anonid", "name-btn");
  EventUtils.synthesizeMouseAtCenter(nameSorter, { }, gManagerWindow);

  check_order([
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test1@tests.mozilla.org"
  ]);

  run_next_test();
});
