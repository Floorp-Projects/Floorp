// Tests for the Find In Page UI

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  let menu = document.getElementById("identity-container");
  let item = document.getElementById("pageaction-findinpage");
  let navigator = document.getElementById("content-navigator");

  // Open and close the find toolbar

  getIdentityHandler().show();
  ok(!menu.hidden, "Site menu is open");
  ok(!navigator.isActive, "Toolbar is closed");

  EventUtils.sendMouseEvent({ type: "click" }, item);
  ok(menu.hidden, "Site menu is closed");
  ok(navigator.isActive, "Toolbar is open");

  EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  ok(menu.hidden, "Site menu is closed");
  ok(!navigator.isActive, "Toolbar is closed");
}
