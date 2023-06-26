/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const clipboard = SpecialPowers.Services.clipboard;
const clipboardTypes = [
  clipboard.kGlobalClipboard,
  clipboard.kSelectionClipboard,
  clipboard.kFindClipboard,
  clipboard.kSelectionCache,
];

function cleanupAllClipboard() {
  clipboardTypes.forEach(function (type) {
    if (clipboard.isClipboardTypeSupported(type)) {
      info(`cleanup clipboard ${type}`);
      clipboard.emptyClipboard(type);
    }
  });
}

function generateRandomString() {
  return "random number: " + Math.random();
}

function generateNewTransferable(aFlavor, aStr) {
  let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor(aFlavor);

  let supportsStr = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  supportsStr.data = aStr;
  trans.setTransferData(aFlavor, supportsStr);

  return trans;
}

function addStringToTransferable(aFlavor, aStr, aTrans) {
  aTrans.addDataFlavor(aFlavor);

  let supportsStr = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  supportsStr.data = aStr;
  aTrans.setTransferData(aFlavor, supportsStr);
}

function writeStringToClipboard(
  aStr,
  aFlavor,
  aClipboardType,
  aClipboardOwner = null,
  aAsync = false
) {
  let trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor(aFlavor);

  let supportsStr = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  supportsStr.data = aStr;
  trans.setTransferData(aFlavor, supportsStr);

  if (aAsync) {
    let request = clipboard.asyncSetData(aClipboardType);
    request.setData(trans, aClipboardOwner);
    return;
  }

  clipboard.setData(trans, aClipboardOwner, aClipboardType);
}

function writeRandomStringToClipboard(
  aFlavor,
  aClipboardType,
  aClipboardOwner = null,
  aAsync = false
) {
  let randomString = generateRandomString();
  writeStringToClipboard(
    randomString,
    aFlavor,
    aClipboardType,
    aClipboardOwner,
    aAsync
  );
  return randomString;
}

function getClipboardData(aFlavor, aClipboardType) {
  var trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor(aFlavor);
  clipboard.getData(trans, aClipboardType);

  try {
    var data = {};
    trans.getTransferData(aFlavor, data);
    return data.value.QueryInterface(SpecialPowers.Ci.nsISupportsString).data;
  } catch (ex) {
    // If the clipboard is empty getTransferData will throw.
    return null;
  }
}
