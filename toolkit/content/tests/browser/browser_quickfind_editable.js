const PAGE =
  "data:text/html,<div contenteditable>foo</div><input><textarea></textarea>";
const DESIGNMODE_PAGE =
  "data:text/html,<body onload='document.designMode=\"on\";'>";
const HOTKEYS = ["/", "'"];

async function test_hotkeys(browser, expected) {
  let findbar = await gBrowser.getFindBar();
  for (let key of HOTKEYS) {
    is(findbar.hidden, true, "findbar is hidden");
    await BrowserTestUtils.sendChar(key, gBrowser.selectedBrowser);
    is(
      findbar.hidden,
      expected,
      "findbar should" + (expected ? "" : " not") + " be hidden"
    );
    if (!expected) {
      await closeFindbarAndWait(findbar);
    }
  }
}

async function focus_element(browser, query) {
  await SpecialPowers.spawn(browser, [query], async function focus(query) {
    let element = content.document.querySelector(query);
    element.focus();
  });
}

add_task(async function test_hotkey_on_editable_element() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function do_tests(browser) {
      await test_hotkeys(browser, false);
      const ELEMENTS = ["div", "input", "textarea"];
      for (let elem of ELEMENTS) {
        await focus_element(browser, elem);
        await test_hotkeys(browser, true);
        await focus_element(browser, ":root");
        await test_hotkeys(browser, false);
      }
    }
  );
});

add_task(async function test_hotkey_on_designMode_document() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: DESIGNMODE_PAGE,
    },
    async function do_tests(browser) {
      await test_hotkeys(browser, true);
    }
  );
});
