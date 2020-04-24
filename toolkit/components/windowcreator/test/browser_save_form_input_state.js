/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

var textareas = ["a-textbox", "a-prefilled-textbox"];
var textboxes = ["a-text", "a-prefilled-text"];

/**
 * Ported from mochitest, this test verifies that form state is saved by our
 * webbrowserpersist code. See bug 293834 for context.
 */
add_task(async function checkFormStateSaved() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "file_form_state.html",
    async browser => {
      await SpecialPowers.spawn(browser, [{ textareas, textboxes }], fillform);
      let fileURISpec = await new Promise((resolve, reject) => {
        let stack = Components.stack.caller;
        browser.frameLoader.startPersistence(null, {
          onDocumentReady(document) {
            // Note that 'document' here is going to be an nsIWebBrowserPersistDocument,
            // not a regular DOM document.
            resolve(persistDocument(document));
          },
          onError(status) {
            reject(
              Components.Exception(
                "saveBrowser failed asynchronously in startPersistence",
                status,
                stack
              )
            );
          },
        });
      });
      await BrowserTestUtils.withNewTab(fileURISpec, async otherBrowser => {
        await SpecialPowers.spawn(
          otherBrowser,
          [{ textareas, textboxes }],
          checkform
        );
      });
    }
  );
});

// eslint-disable-next-line no-shadow
function fillform({ textareas, textboxes }) {
  let doc = content.document;
  for (let i in textareas) {
    doc.getElementById(textareas[i]).textContent += "form state";
  }
  for (let i in textboxes) {
    doc.getElementById(textboxes[i]).value += "form state";
  }
  doc.getElementById("a-checkbox").checked = true;
  doc.getElementById("radioa").checked = true;
  doc.getElementById("aselect").selectedIndex = 0;
}

// eslint-disable-next-line no-shadow
function checkform({ textareas, textboxes }) {
  let doc = content.document;
  for (let i in textareas) {
    var textContent = doc.getElementById(textareas[i]).textContent;
    Assert.ok(
      /form\s+state/m.test(textContent),
      "Modified textarea " + textareas[i] + " form state should be preserved!"
    );
  }
  for (let i in textboxes) {
    var value = doc.getElementById(textboxes[i]).value;
    Assert.ok(
      /form\s+state/m.test(value),
      "Modified textbox " + textboxes[i] + " form state should be preserved!"
    );
  }
  Assert.ok(
    doc.getElementById("a-checkbox").checked,
    "Modified checkbox checked state should be preserved!"
  );
  Assert.ok(
    doc.getElementById("radioa").checked,
    "Modified radio checked state should be preserved!"
  );
  Assert.equal(
    doc.getElementById("aselect").selectedIndex,
    0,
    "Modified select selected index should be preserved"
  );
}

function getTempDir() {
  return Services.dirsvc.get("TmpD", Ci.nsIFile);
}

function persistDocument(aDoc) {
  const nsIWBP = Ci.nsIWebBrowserPersist;
  const persistFlags =
    nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
    nsIWBP.PERSIST_FLAGS_FROM_CACHE |
    nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;
  const encodingFlags = nsIWBP.ENCODE_FLAGS_ENCODE_BASIC_ENTITIES;

  var file = getTempDir();
  file.append("bug293834-serialized.html");

  var persist = Cc[
    "@mozilla.org/embedding/browser/nsWebBrowserPersist;1"
  ].createInstance(Ci.nsIWebBrowserPersist);
  persist.progressListener = null;
  persist.persistFlags = persistFlags;
  const kWrapColumn = 80;
  var folder = getTempDir();
  folder.append("bug293834-serialized");
  persist.saveDocument(
    aDoc,
    file,
    folder,
    "text/html",
    encodingFlags,
    kWrapColumn
  );
  return Services.io.newFileURI(file).spec;
}
