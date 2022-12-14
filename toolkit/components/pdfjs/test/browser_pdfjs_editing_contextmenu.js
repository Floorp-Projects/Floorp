/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RELATIVE_DIR = "toolkit/components/pdfjs/test/";
const TESTROOT = "http://example.com/browser/" + RELATIVE_DIR;

// This is a modified version from browser_contextmenuFillLogins.js.
async function openContextMenuAt(browser, x, y) {
  const doc = browser.ownerDocument;
  const contextMenu = doc.getElementById("contentAreaContextMenu");

  const contextMenuShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );

  // Synthesize a contextmenu event to actually open the context menu.
  await BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "contextmenu",
      button: 2,
    },
    browser
  );

  await contextMenuShownPromise;
  return contextMenu;
}

/**
 * Open a context menu and get the pdfjs entries
 * @param {Object} browser
 * @param {Object} box
 * @returns {Map<string,HTMLElement>} the pdfjs menu entries.
 */
async function getContextMenuItems(browser, box) {
  return new Promise(resolve => {
    setTimeout(async () => {
      const { x, y, width, height } = box;
      const menuitems = [
        "context-pdfjs-undo",
        "context-pdfjs-redo",
        "context-sep-pdfjs-redo",
        "context-pdfjs-cut",
        "context-pdfjs-copy",
        "context-pdfjs-paste",
        "context-pdfjs-delete",
        "context-pdfjs-selectall",
        "context-sep-pdfjs-selectall",
      ];

      await openContextMenuAt(browser, x + width / 2, y + height / 2);
      const results = new Map();
      const doc = browser.ownerDocument;
      for (const menuitem of menuitems) {
        const item = doc.getElementById(menuitem);
        results.set(menuitem, item || null);
      }

      resolve(results);
    }, 0);
  });
}

/**
 * Open a context menu on the element corresponding to the given selector
 * and returs the pdfjs menu entries.
 * @param {Object} browser
 * @param {string} selector
 * @returns {Map<string,HTMLElement>} the pdfjs menu entries.
 */
async function getContextMenuItemsOn(browser, selector) {
  const box = await SpecialPowers.spawn(browser, [selector], async function(
    selector
  ) {
    const element = content.document.querySelector(selector);
    const { x, y, width, height } = element.getBoundingClientRect();
    return { x, y, width, height };
  });
  return getContextMenuItems(browser, box);
}

/**
 * Hide the context menu.
 * @param {Object} browser
 */
async function hideContextMenu(browser) {
  await new Promise(resolve =>
    setTimeout(async () => {
      const doc = browser.ownerDocument;
      const contextMenu = doc.getElementById("contentAreaContextMenu");

      const popupHiddenPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      contextMenu.hidePopup();
      await popupHiddenPromise;
      resolve();
    }, 0)
  );
}

async function clickOnItem(browser, items, entry) {
  const editingPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "editingaction",
    false,
    null,
    true
  );
  items.get(entry).click();
  await editingPromise;
}

/**
 * Asserts that the enabled pdfjs menuitems are the expected ones.
 * @param {Map<string,HTMLElement>} menuitems
 * @param {Array<string>} expected
 */
function assertMenuitems(menuitems, expected) {
  Assert.deepEqual(
    [...menuitems.values()]
      .filter(
        elmt =>
          !elmt.id.includes("-sep-") &&
          !elmt.hidden &&
          elmt.getAttribute("disabled") === "false"
      )
      .map(elmt => elmt.id),
    expected
  );
}

// Text copy, paste, undo, redo, delete and select all in using the context
// menu.
add_task(async function test() {
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );

  // Make sure pdf.js is the default handler.
  is(
    handlerInfo.alwaysAskBeforeHandling,
    false,
    "pdf handler defaults to always-ask is false"
  );
  is(
    handlerInfo.preferredAction,
    Ci.nsIHandlerInfo.handleInternally,
    "pdf handler defaults to internal"
  );

  info("Pref action: " + handlerInfo.preferredAction);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async function(browser) {
      SpecialPowers.clipboardCopyString("");

      await SpecialPowers.pushPrefEnv({
        set: [["pdfjs.annotationEditorMode", 0]],
      });

      // check that PDF is opened with internal viewer
      await waitForPdfJSAllLayers(browser, TESTROOT + "file_pdfjs_test.pdf", [
        [
          "annotationEditorLayer",
          "annotationLayer",
          "textLayer",
          "canvasWrapper",
        ],
        ["annotationEditorLayer", "textLayer", "canvasWrapper"],
      ]);

      const spanBox = await getSpanBox(browser, "and found references");
      let menuitems = await getContextMenuItems(browser, spanBox);

      // Nothing have been edited, hence the context menu doesn't contain any
      // pdf entries.
      Assert.ok(
        [...menuitems.values()].every(elmt => elmt.hidden),
        "No visible pdf menuitem"
      );
      await hideContextMenu(browser);

      await enableEditor(browser, "FreeText");
      await addFreeText(browser, "hello", spanBox);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 0
      );

      await TestUtils.waitForTick();

      Assert.equal(await countElements(browser, ".freeTextEditor"), 1);

      // Unselect.
      await escape(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".selectedEditor")) !== 1
      );

      Assert.equal(await countElements(browser, ".selectedEditor"), 0);

      menuitems = await getContextMenuItems(browser, spanBox);
      assertMenuitems(menuitems, [
        "context-pdfjs-undo", // Last created editor is undoable
        "context-pdfjs-selectall", // and selectable.
      ]);
      // Undo.
      await clickOnItem(browser, menuitems, "context-pdfjs-undo");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 1
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been removed"
      );

      menuitems = await getContextMenuItems(browser, spanBox);

      // The editor removed thanks to "undo" is now redoable
      assertMenuitems(menuitems, ["context-pdfjs-redo"]);
      await clickOnItem(browser, menuitems, "context-pdfjs-redo");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 0
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been added back"
      );

      await clickOn(browser, "#pdfjs_internal_editor_0");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_0"
      );

      assertMenuitems(menuitems, [
        "context-pdfjs-undo",
        "context-pdfjs-cut",
        "context-pdfjs-copy",
        "context-pdfjs-delete",
        "context-pdfjs-selectall",
      ]);

      await clickOnItem(browser, menuitems, "context-pdfjs-cut");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 1
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been cut"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      assertMenuitems(menuitems, ["context-pdfjs-undo", "context-pdfjs-paste"]);

      await clickOnItem(browser, menuitems, "context-pdfjs-paste");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 0
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been pasted"
      );

      await clickOn(browser, "#pdfjs_internal_editor_1");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_1"
      );

      assertMenuitems(menuitems, [
        "context-pdfjs-undo",
        "context-pdfjs-cut",
        "context-pdfjs-copy",
        "context-pdfjs-paste",
        "context-pdfjs-delete",
        "context-pdfjs-selectall",
      ]);

      await clickOnItem(browser, menuitems, "context-pdfjs-delete");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 1
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "The FreeText editor must have been deleted"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      await clickOnItem(browser, menuitems, "context-pdfjs-paste");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 0
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        1,
        "The FreeText editor must have been pasted"
      );

      await clickOn(browser, "#pdfjs_internal_editor_2");
      menuitems = await getContextMenuItemsOn(
        browser,
        "#pdfjs_internal_editor_2"
      );

      await clickOnItem(browser, menuitems, "context-pdfjs-copy");
      await clickOnItem(browser, menuitems, "context-pdfjs-paste");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 1
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        2,
        "The FreeText editor must have been pasted"
      );

      menuitems = await getContextMenuItems(browser, spanBox);
      await clickOnItem(browser, menuitems, "context-pdfjs-selectall");
      await clickOnItem(browser, menuitems, "context-pdfjs-delete");
      await hideContextMenu(browser);

      await BrowserTestUtils.waitForCondition(
        async () => (await countElements(browser, ".freeTextEditor")) !== 2
      );

      Assert.equal(
        await countElements(browser, ".freeTextEditor"),
        0,
        "All the FreeText editors must have been deleted"
      );

      await SpecialPowers.spawn(browser, [], async function() {
        var viewer = content.wrappedJSObject.PDFViewerApplication;
        await viewer.close();
      });
    }
  );
});
