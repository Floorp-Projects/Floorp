/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutWindowsMessages = null;

function refreshMessages() {
  let windowMessages = {};
  let windowTitles = {};
  AboutWindowsMessages.getMessages(window, windowMessages, windowTitles);
  let windowsDiv = document.getElementById("windows-div");
  windowsDiv.innerHTML = "";
  const templateCard = document.querySelector("template[name=window-card]");
  for (let i = 0; i < windowTitles.value.length; ++i) {
    let windowCard = templateCard.content
      .cloneNode(true)
      .querySelector("details");
    // open the current window by default
    windowCard.open = i === 0;
    let summary = windowCard.querySelector("summary");
    let titleSpan = summary.querySelector("h3.window-card-title");
    titleSpan.appendChild(document.createTextNode(windowTitles.value[i]));
    titleSpan.classList.toggle("current-window", windowCard.open);
    let copyButton = summary.querySelector("button");
    copyButton.addEventListener("click", async e => {
      e.target.disabled = true;
      await copyMessagesToClipboard(e);
      e.target.disabled = false;
    });
    let innerUl = document.createElement("ul");
    for (let j = 0; j < windowMessages.value[i].length; ++j) {
      let innerLi = document.createElement("li");
      innerLi.className = "message";
      innerLi.innerText = windowMessages.value[i][j];
      innerUl.appendChild(innerLi);
    }
    windowCard.appendChild(innerUl);
    windowsDiv.append(windowCard);
  }
}

async function copyMessagesToClipboard(event) {
  const details = event.target.parentElement.parentElement;
  // Avoid copying the window name as it is Category 3 data,
  // and only useful for the user to identify which window
  // is which.
  const messagesText =
    Array.from(details.querySelector("ul").children)
      .map(li => li.innerText)
      .join("\n") + "\n";

  await navigator.clipboard.writeText(messagesText);
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
  console.error(ex);
}
