/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 625465 - Items in list view should launch description view with a single click on an add-on entry

var gManagerWindow;
var gList;

function test() {
  waitForExplicitFinish();

  var gProvider = new MockProvider();
  for (let i = 1; i <= 30; i++) {
    gProvider.createAddons([{
      id: "test" + i + "@tests.mozilla.org",
      name: "Test add-on " + i,
      description: "foo"
    }]);
  }

  open_manager("addons://list/extension", function(aManager) {
    gManagerWindow = aManager;
    gList = gManagerWindow.document.getElementById("addon-list");

    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, finish);
}


function get_item_content_pos(aItem) {
  return {
    x: (aItem.boxObject.x - gList.boxObject.x) + 10,
    y: (aItem.boxObject.y - gList.boxObject.y - gList._scrollbox.scrollTop) + 10
  };
}

function get_item_index(aItem) {
  for (let i = 0; i < gList.childElementCount; i++) {
    if (gList.childNodes[i] == aItem)
      return i;
  }
  return -1;
}

function mouseover_item(aItemNum, aCallback) {
  var item = get_addon_element(gManagerWindow, "test" + aItemNum + "@tests.mozilla.org");
  var itemIndex = get_item_index(item);
  gList.ensureElementIsVisible(item);
  if (aItemNum == 1)
    is(gList.selectedIndex, -1, "Should not initially have an item selected");

  info("Moving mouse over item: " + item.value);
  var pos = get_item_content_pos(item);
  EventUtils.synthesizeMouse(gList, pos.x, pos.y, {type: "mousemove"}, gManagerWindow);
  executeSoon(function() {
    is(gManagerWindow.gViewController.currentViewId, "addons://list/extension", "Should still be in list view");
    is(gList.selectedIndex, itemIndex, "Correct item should be selected");
  
    aCallback();
  });
}

add_test(function() {
  var i = 1;
  function test_next_item() {
    if (i < 10) {
      mouseover_item(i, function() {
        i++
        test_next_item();
      });
    } else {
      run_next_test();
    }
  }
  test_next_item();
});


add_test(function() {
  gList.selectedIndex = 1;
  var item = gList.selectedItem;
  gList.ensureElementIsVisible(item);
  var pos = get_item_content_pos(item);
  EventUtils.synthesizeMouse(gList, pos.x, pos.y, {type: "mousemove"}, gManagerWindow);
  executeSoon(function() {
    var scrollDelta = item.boxObject.height * 2;
    info("Scrolling by " + scrollDelta + "px (2 items)");
    EventUtils.synthesizeMouseScroll(gList, pos.x, pos.y, {type: "MozMousePixelScroll", delta: scrollDelta}, gManagerWindow);
    setTimeout(function() {
      var item = gList.selectedItem;
      var itemIndex = get_item_index(item);
      is(itemIndex, 3, "Correct item should be selected");
      run_next_test();
    }, 100);
  });
});
