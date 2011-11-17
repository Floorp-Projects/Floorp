let gTests = [];
let gCurrentTest = null;
let Panels = [AllPagesList, HistoryList, BookmarkList];

let removedForTestButtons = [];

function test() {
  waitForExplicitFinish();

  // Make sure we start the test with less than a full menu
  let menu = document.getElementById("appmenu");
  while (menu.children.length > 5)
    removedForTestButtons.push(menu.removeChild(menu.lastChild));

  setTimeout(runNextTest, 200);
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  } else {
    // Add back any removed buttons
    let menu = document.getElementById("appmenu");
    removedForTestButtons.forEach(function(aButton) {
      menu.appendChild(aButton);
    });

    // Close the awesome panel just in case
    AwesomeScreen.activePanel = null;
    finish();
  }
}

gTests.push({
// This test will keep adding buttons and checking the result until there are a
// total of 9 buttons, then it will (one at a time) hide 3 items and check the
// result again.

  desc: "Test for showing the application menu",
  newButtons: [],
  hidden: 0,

  run: function() {
    addEventListener("PopupChanged", gCurrentTest.popupOpened, false)
    CommandUpdater.doCommand("cmd_menu");
  },

  addButton: function() {
    info("Adding a new button\n");
    let menu = document.getElementById("appmenu");
    let newButton = menu.children[0].cloneNode(true);
    menu.appendChild(newButton);
    gCurrentTest.newButtons.push(newButton);
  },

  popupOpened: function() {
    removeEventListener("PopupChanged", gCurrentTest.popupOpened, false);
    let menu = document.getElementById("appmenu");
    ok(!menu.hidden, "App menu is shown");

    let more = document.getElementById("appmenu-more-button");
    if (menu.children.length > 6) {
      ok(!!more, "More button is shown");
      addEventListener("PopupChanged", gCurrentTest.moreShown, false);
      more.click();
    } else {
      ok(!more, "More button is hidden");
      addEventListener("PopupChanged", gCurrentTest.popupClosed, false);
      EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
    }
  },

  popupClosed: function() {
    removeEventListener("PopupChanged", gCurrentTest.popupClosed, false);
    let menu = document.getElementById("appmenu");
    ok(document.getElementById("appmenu").hidden, "Esc hides menus");
    if (menu.children.length < 9) {
      gCurrentTest.addButton();
      gCurrentTest.run();
    } else {
      menu.children[gCurrentTest.hidden].hidden = true;
      gCurrentTest.hidden++;
      addEventListener("PopupChanged", gCurrentTest.menuitemHidden, false)
      CommandUpdater.doCommand("cmd_menu");
    }
  },

  moreShown: function(aEvent) {
    // AppMenu hiding
    if (!aEvent.detail)
      return;

    let menu = document.getElementById("appmenu");
    ok(document.getElementById("appmenu").hidden, "Clicking more button hides menu");

    removeEventListener("PopupChanged", gCurrentTest.moreShown, false);
    let listbox = document.getElementById("appmenu-overflow-commands");
    is(listbox.childNodes.length, (menu.childNodes.length - 5), "Menu popup only shows overflow children");

    addEventListener("PopupChanged", gCurrentTest.popupClosed, false);
    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  menuitemHidden: function() {
    removeEventListener("PopupChanged", gCurrentTest.menuitemHidden, false);
    let menu = document.getElementById("appmenu");
    ok(!document.getElementById("appmenu").hidden, "App menu is shown");

    let more = document.getElementById("appmenu-more-button");
    if (menu.children.length - gCurrentTest.hidden > 6) {
      ok(more, "More button is shown");
      addEventListener("PopupChanged", gCurrentTest.popupClosed, false);
    } else {
      ok(!more, "More button is hidden");
      addEventListener("PopupChanged", gCurrentTest.popupClosedAgain, false);
    }
    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  popupClosedAgain: function() {
    removeEventListener("PopupChanged", gCurrentTest.popupClosedAgain, false)
    let menu = document.getElementById("appmenu");
    while (gCurrentTest.hidden > 0) {
      gCurrentTest.hidden--;
      menu.children[gCurrentTest.hidden].hidden = false;
    }

    gCurrentTest.newButtons.forEach(function(aButton) {
      menu.removeChild(aButton);
    });

    runNextTest();
  }
});
