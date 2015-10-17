const PAGE = "data:text/html,<div contenteditable>foo</div><input><textarea></textarea>";
const DESIGNMODE_PAGE = "data:text/html,<body onload='document.designMode=\"on\";'>";
const HOTKEYS = ["/", "'"];

function* test_hotkeys(browser, expected) {
  let findbar = gBrowser.getFindBar();
  for (let key of HOTKEYS) {
    is(findbar.hidden, true, "findbar is hidden");
    EventUtils.synthesizeKey(key, {}, browser.contentWindow);
    is(findbar.hidden, expected, "findbar should" + (expected ? "" : " not") + " be hidden");
    if (!expected) {
      yield closeFindbarAndWait(findbar);
    }
  }
}

function* focus_element(browser, query) {
  yield ContentTask.spawn(browser, query, function* focus(query) {
    let element = content.document.querySelector(query);
    element.focus();
  });
}

add_task(function* test_hotkey_on_editable_element() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, function* do_tests(browser) {
    yield test_hotkeys(browser, false);
    const ELEMENTS = ["div", "input", "textarea"];
    for (let elem of ELEMENTS) {
      yield focus_element(browser, elem);
      yield test_hotkeys(browser, true);
      yield focus_element(browser, ":root");
      yield test_hotkeys(browser, false);
    }
  });
});

add_task(function* test_hotkey_on_designMode_document() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: DESIGNMODE_PAGE
  }, function* do_tests(browser) {
    yield test_hotkeys(browser, true);
  });
});
