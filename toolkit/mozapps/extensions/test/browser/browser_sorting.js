/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that sorting of add-ons works correctly
// (this test uses the list view, even though it no longer has sort buttons - see bug 623207)

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

function set_order(aSortBy, aAscending) {
  var list = gManagerWindow.document.getElementById("addon-list");
  var elements = [];
  var node = list.firstChild;
  while (node) {
    elements.push(node);
    node = node.nextSibling;
  }
  gManagerWindow.sortElements(elements, aSortBy, aAscending);
  elements.forEach(function(aElement) {
    list.appendChild(aElement);
  });
}

function check_order(aExpectedOrder) {
  var order = [];
  var list = gManagerWindow.document.getElementById("addon-list");
  var node = list.firstChild;
  while (node) {
    var id = node.getAttribute("value");
    if (id && id.substring(id.length - 18) == "@tests.mozilla.org")
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

// Tests that switching to date ordering works
add_test(function() {
  set_order("updateDate", false);

  check_order([
    "test5@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org"
  ]);

  set_order("updateDate", true);

  check_order([
    "test4@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org"
  ]);

  run_next_test();
});

// Tests that switching to name ordering works
add_test(function() {
  set_order("name", true);

  check_order([
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test1@tests.mozilla.org"
  ]);

  set_order("name", false);

  check_order([
    "test1@tests.mozilla.org",
    "test5@tests.mozilla.org",
    "test3@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test2@tests.mozilla.org"
  ]);

  run_next_test();
});
