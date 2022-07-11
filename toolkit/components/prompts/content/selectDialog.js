/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Defined in dialog.xml.
/* globals centerWindowOnScreen:false, moveToAlertPosition:false */

var propBag, listBox, args;

function onDCL() {
  propBag = window.arguments[0]
    .QueryInterface(Ci.nsIWritablePropertyBag2)
    .QueryInterface(Ci.nsIWritablePropertyBag);

  // Convert to a JS object
  let args = {};
  for (let prop of propBag.enumerator) {
    args[prop.name] = prop.value;
  }

  let promptType = propBag.getProperty("promptType");
  if (promptType != "select") {
    Cu.reportError("selectDialog opened for unknown type: " + promptType);
    window.close();
  }

  // Default to canceled.
  propBag.setProperty("ok", false);

  document.title = propBag.getProperty("title");

  let text = propBag.getProperty("text");
  document.getElementById("info.txt").setAttribute("value", text);

  let items = propBag.getProperty("list");
  listBox = document.getElementById("list");

  for (let i = 0; i < items.length; i++) {
    let str = items[i];
    if (str == "") {
      str = "<>";
    }
    listBox.appendItem(str);
    listBox.getItemAtIndex(i).addEventListener("dblclick", dialogDoubleClick);
  }
  listBox.selectedIndex = 0;
}

function onLoad() {
  listBox.focus();

  document.addEventListener("dialogaccept", dialogOK);
  // resize the window to the content
  window.sizeToContent();

  // Move to the right location
  moveToAlertPosition();
  centerWindowOnScreen();

  // play sound
  try {
    if (!args.openedWithTabDialog) {
      Cc["@mozilla.org/sound;1"]
        .getService(Ci.nsISound)
        .playEventSound(Ci.nsISound.EVENT_SELECT_DIALOG_OPEN);
    }
  } catch (e) {}

  Services.obs.notifyObservers(window, "select-dialog-loaded");
}

function dialogOK() {
  propBag.setProperty("selected", listBox.selectedIndex);
  propBag.setProperty("ok", true);
}

function dialogDoubleClick() {
  dialogOK();
  window.close();
}

document.addEventListener("DOMContentLoaded", onDCL);
window.addEventListener("load", onLoad, { once: true });
