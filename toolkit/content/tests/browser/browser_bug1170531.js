// Test for bug 1170531
// https://bugzilla.mozilla.org/show_bug.cgi?id=1170531

add_task(function* () {
  // Get a bunch of DOM nodes
  let editMenu = document.getElementById("edit-menu");
  let menuPopup = editMenu.menupopup;

  let closeMenu = function(aCallback) {
    if (OS.Constants.Sys.Name == "Darwin") {
      executeSoon(aCallback);
      return;
    }

    menuPopup.addEventListener("popuphidden", function onPopupHidden() {
      menuPopup.removeEventListener("popuphidden", onPopupHidden, false);
      executeSoon(aCallback);
    }, false);

    executeSoon(function() {
      editMenu.open = false;
    });
  };

  let openMenu = function(aCallback) {
    if (OS.Constants.Sys.Name == "Darwin") {
      goUpdateGlobalEditMenuItems();
      // On OSX, we have a native menu, so it has to be updated. In single process browsers,
      // this happens synchronously, but in e10s, we have to wait for the main thread
      // to deal with it for us. 1 second should be plenty of time.
      setTimeout(aCallback, 1000);
      return;
    }

    menuPopup.addEventListener("popupshown", function onPopupShown() {
      menuPopup.removeEventListener("popupshown", onPopupShown, false);
      executeSoon(aCallback);
    }, false);

    executeSoon(function() {
      editMenu.open = true;
    });
  };

  yield BrowserTestUtils.withNewTab({ gBrowser: gBrowser, url: "about:blank" }, function* (browser) {
    let menu_cut_disabled, menu_copy_disabled;

    yield BrowserTestUtils.loadURI(browser, "data:text/html,<div>hello!</div>");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    yield new Promise(openMenu);
    menu_cut_disabled = menuPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, false, "menu_cut should be enabled");
    menu_copy_disabled = menuPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, false, "menu_copy should be enabled");
    yield new Promise(closeMenu);

    yield BrowserTestUtils.loadURI(browser, "data:text/html,<div contentEditable='true'>hello!</div>");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    yield new Promise(openMenu);
    menu_cut_disabled = menuPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, false, "menu_cut should be enabled");
    menu_copy_disabled = menuPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, false, "menu_copy should be enabled");
    yield new Promise(closeMenu);

    yield BrowserTestUtils.loadURI(browser, "about:preferences");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    yield new Promise(openMenu);
    menu_cut_disabled = menuPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, true, "menu_cut should be disabled");
    menu_copy_disabled = menuPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, true, "menu_copy should be disabled");
    yield new Promise(closeMenu);
  });
});
