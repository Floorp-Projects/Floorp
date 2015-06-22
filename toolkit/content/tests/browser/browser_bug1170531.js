// Test for bug 1170531
// https://bugzilla.mozilla.org/show_bug.cgi?id=1170531

add_task(function* () {
  yield BrowserTestUtils.withNewTab({ gBrowser: gBrowser, url: "about:blank" }, function* (browser) {
    let menu_EditPopup = document.getElementById("menu_EditPopup");
    let menu_cut_disabled, menu_copy_disabled;

    yield BrowserTestUtils.loadURI(browser, "data:text/html,<div>hello!</div>");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    goUpdateGlobalEditMenuItems();
    menu_cut_disabled = menu_EditPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, false, "menu_cut should be enabled");
    menu_copy_disabled = menu_EditPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, false, "menu_copy should be enabled");

    yield BrowserTestUtils.loadURI(browser, "data:text/html,<div contentEditable='true'>hello!</div>");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    goUpdateGlobalEditMenuItems();
    menu_cut_disabled = menu_EditPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, false, "menu_cut should be enabled");
    menu_copy_disabled = menu_EditPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, false, "menu_copy should be enabled");

    yield BrowserTestUtils.loadURI(browser, "about:preferences");
    browser.focus();
    yield new Promise(resolve => waitForFocus(resolve, window));
    goUpdateGlobalEditMenuItems();
    menu_cut_disabled = menu_EditPopup.querySelector("#menu_cut").getAttribute('disabled') == "true";
    is(menu_cut_disabled, true, "menu_cut should be disabled");
    menu_copy_disabled = menu_EditPopup.querySelector("#menu_copy").getAttribute('disabled') == "true";
    is(menu_copy_disabled, true, "menu_copy should be disabled");
  });
});
