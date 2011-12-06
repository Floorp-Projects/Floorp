Components.utils.import("resource://gre/modules/Services.jsm");

var gWin;

function test() {
  waitForExplicitFinish();
  gWin = Services.ww.openWindow(window, "chrome://browser/content/localePicker.xul", "_browser", "chrome,dialog=no,all", null);
  gWin.addEventListener("load", onload, false);
}

function onload(aEvent) {
  gWin.removeEventListener("load", onload, false);
  ok(true, "Locale picker is opened.");
  setTimeout(afterLoad, 0);
}

function afterLoad() {
  let ui = gWin.LocaleUI;
  is(ui.selectedPanel, ui.pickerpage, "Picker page is selected.");

  ui.selectedPanel = ui.mainPage;
  is(ui.selectedPanel, ui.mainPage, "Select the main page.");
  sendEscape();
  is(ui.selectedPanel, ui.mainPage, "Main page is still selected (escape key does nothing).");

  ui.selectedPanel = ui.installerPage;
  is(ui.selectedPanel, ui.installerPage, "Select the installer page.");
  sendEscape();
  is(ui.selectedPanel, ui.pickerpage, "Escape key goes back to the picker page.");

  gWin.addEventListener("unload", windowClosed, false);
  sendEscape();
}

function windowClosed() {
  gWin.removeEventListener("unload", windowClosed, false);
  ok(true, "Locale picker is closed.");
  finish();
}

function sendEscape() {
  info("Sending escape key.");
  EventUtils.synthesizeKey("VK_ESCAPE", { type: "keypress" }, gWin);
}
