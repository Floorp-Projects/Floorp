/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";
const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");

function getString(clipboard) {
  var str = "";

  // Create transferable that will transfer the text.
  var trans = Cc["@mozilla.org/widget/transferable;1"]
                        .createInstance(Ci.nsITransferable);
  trans.init(null);
  trans.addDataFlavor("text/unicode");

  clipboard.getData(trans, Ci.nsIClipboard.kGlobalClipboard);

  try {
    var data = {};
    var dataLen = {};
    trans.getTransferData("text/unicode", data, dataLen);

    if (data) {
      data = data.value.QueryInterface(Ci.nsISupportsString);
      str = data.data.substring(0, dataLen.value / 2);
    }
  } catch (ex) {
    // If the clipboard is empty getTransferData will throw.
  }

  return str;
}

add_task(async function test_clipboard() {
  let clipboard = Cc['@mozilla.org/widget/clipboard;1']
                  .getService(Ci.nsIClipboard);

  // Test copy.
  const data = "random number: " + Math.random();
  let helper = Cc['@mozilla.org/widget/clipboardhelper;1']
               .getService(Ci.nsIClipboardHelper);
  helper.copyString(data);
  equal(getString(clipboard), data, 'Data was successfully copied.');

  clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
  equal(getString(clipboard), '', 'Data was successfully cleared.');
});
