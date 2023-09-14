/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from clipboard_helper.js */

"use strict";

function getLoadContext() {
  return SpecialPowers.wrap(window).docShell.QueryInterface(Ci.nsILoadContext);
}

// Get clipboard data to paste.
function paste(clipboard) {
  let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(getLoadContext());
  trans.addDataFlavor("text/plain");
  clipboard.getData(trans, Ci.nsIClipboard.kGlobalClipboard);
  let str = SpecialPowers.createBlankObject();
  try {
    trans.getTransferData("text/plain", str);
  } catch (e) {
    str = "";
  }
  if (str) {
    str = str.value.QueryInterface(Ci.nsISupportsString);
    if (str) {
      str = str.data;
    }
  }
  return str;
}

add_setup(function init() {
  cleanupAllClipboard();
});

/* Test for bug 948065 */
add_task(function test_copy() {
  // Test copy.
  const data = "random number: " + Math.random();
  let helper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
    Ci.nsIClipboardHelper
  );
  helper.copyString(data);
  is(paste(clipboard), data, "Data was successfully copied.");

  clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
  is(paste(clipboard), "", "Data was successfully cleared.");

  cleanupAllClipboard();
});

/* Tests for bug 1834073 */
clipboardTypes.forEach(function (clipboardType) {
  if (clipboard.isClipboardTypeSupported(clipboardType)) {
    add_task(function test_clipboard_apis() {
      info(`Test clipboard apis for type ${clipboardType}`);

      // Set clipboard data
      let str;
      try {
        str = writeRandomStringToClipboard("text/plain", clipboardType);
      } catch (e) {
        ok(
          false,
          `setData should not throw error for clipboard type ${clipboardType}`
        );
      }

      // Test hasDataMatchingFlavors
      try {
        ok(
          clipboard.hasDataMatchingFlavors(["text/plain"], clipboardType),
          `Test hasDataMatchingFlavors for clipboard type ${clipboardType}`
        );
      } catch (e) {
        ok(
          false,
          `hasDataMatchingFlavors should not throw error for clipboard type ${clipboardType}`
        );
      }

      // Test getData
      try {
        is(
          getClipboardData("text/plain", clipboardType),
          str,
          `Test getData for clipboard type ${clipboardType}`
        );
      } catch (e) {
        ok(
          false,
          `getData should not throw error for clipboard type ${clipboardType}`
        );
      }
    });

    add_task(function test_clipboard_set_empty_string() {
      info(`Test setting empty string to type ${clipboardType}`);

      // Clear clipboard type.
      clipboard.emptyClipboard(clipboardType);
      is(
        getClipboardData("text/plain", clipboardType),
        null,
        `Should get null data on clipboard type ${clipboardType}`
      );
      ok(
        !clipboard.hasDataMatchingFlavors(["text/plain"], clipboardType),
        `Should not have text/plain flavor on clipboard ${clipboardType}`
      );

      // Set text/plain to empty string.
      writeStringToClipboard("", "text/plain", clipboardType);
      // XXX gtk doesn't support get empty text data from clipboard, bug 1852983.
      if (navigator.platform.includes("Linux")) {
        todo_is(
          getClipboardData("text/plain", clipboardType),
          "",
          `Should get empty string on clipboard type ${clipboardType}`
        );
      } else {
        is(
          getClipboardData("text/plain", clipboardType),
          "",
          `Should get empty string on clipboard type ${clipboardType}`
        );
      }
      // XXX android doesn't support setting empty text data to clipboard, bug 1841058.
      if (navigator.userAgent.includes("Android")) {
        todo_is(
          clipboard.hasDataMatchingFlavors(["text/plain"], clipboardType),
          true,
          `Should have text/plain flavor on clipboard ${clipboardType}`
        );
      } else {
        ok(
          clipboard.hasDataMatchingFlavors(["text/plain"], clipboardType),
          `Should have text/plain flavor on clipboard ${clipboardType}`
        );
      }

      // Clear all clipboard data.
      cleanupAllClipboard();
    });
  }
});
