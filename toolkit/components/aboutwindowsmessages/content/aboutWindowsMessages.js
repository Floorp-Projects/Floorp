/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutWindowsMessages = null;

function refreshMessages() {
  let windowMessages = {};
  let windowTitles = {};
  AboutWindowsMessages.getMessages(window, windowMessages, windowTitles);
  let messagesUl = document.getElementById("messages-ul");
  messagesUl.innerHTML = "";
  for (let i = 0; i < windowTitles.value.length; ++i) {
    let titleLi = document.createElement("li");
    let titleTextNode = document.createTextNode(windowTitles.value[i]);
    if (i === 0) {
      // bold the first window since it's the current one
      let b = document.createElement("b");
      b.appendChild(titleTextNode);
      titleLi.appendChild(b);
    } else {
      titleLi.appendChild(titleTextNode);
    }
    let innerUl = document.createElement("ul");
    for (let j = 0; j < windowMessages.value[i].length; ++j) {
      let innerLi = document.createElement("li");
      innerLi.innerText = windowMessages.value[i][j];
      innerUl.appendChild(innerLi);
    }
    titleLi.appendChild(innerUl);
    messagesUl.append(titleLi);
  }
}

function onLoad() {
  refreshMessages();
}

try {
  AboutWindowsMessages = Cc["@mozilla.org/about-windowsmessages;1"].getService(
    Ci.nsIAboutWindowsMessages
  );
  document.addEventListener("DOMContentLoaded", onLoad, { once: true });
} catch (ex) {
  // Do nothing if we fail to create a singleton instance,
  // showing the default no-module message.
  Cu.reportError(ex);
}
