// Tests for the Find In Page UI

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  addEventListener("PopupChanged", popupOpened, false);
  CommandUpdater.doCommand("cmd_menu");
}

function popupOpened() {
  removeEventListener("PopupChanged", popupOpened, false);

  let menu = document.getElementById("appmenu");
  ok(!menu.hidden, "App menu is shown");

  let navigator = document.getElementById("content-navigator");
  ok(!navigator.isActive, "Toolbar is closed");

  addEventListener("PopupChanged", findOpened, false);
  let item = document.getElementsByClassName("appmenu-findinpage-button")[0];
  item.click();
}

function findOpened() {
  removeEventListener("PopupChanged", findOpened, false);

  let menu = document.getElementById("appmenu");
  ok(menu.hidden, "App menu is closed");

  let navigator = document.getElementById("content-navigator");
  ok(navigator.isActive, "Toolbar is open");

  is(navigator._previousButton.disabled, true, "Previous button should be disabled");
  is(navigator._nextButton.disabled, true, "Previous button should be disabled");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  ok(menu.hidden, "Site menu is closed");
  ok(!navigator.isActive, "Toolbar is closed");
}
