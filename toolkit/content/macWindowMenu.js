// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function macWindowMenuDidShow() {
  let frag = document.createDocumentFragment();
  for (let win of Services.wm.getEnumerator("")) {
    if (win.document.documentElement.getAttribute("inwindowmenu") == "false") {
      continue;
    }
    let item = document.createXULElement("menuitem");
    item.setAttribute("label", win.document.title);
    if (win == window) {
      item.setAttribute("checked", "true");
    }
    item.addEventListener("command", () => {
      if (win.windowState == window.STATE_MINIMIZED) {
        win.restore();
      }
      win.focus();
    });
    frag.appendChild(item);
  }
  document.getElementById("windowPopup").appendChild(frag);
}

function macWindowMenuDidHide() {
  let sep = document.getElementById("sep-window-list");
  // Clear old items
  while (sep.nextElementSibling) {
    sep.nextElementSibling.remove();
  }
}

function zoomWindow() {
  if (window.windowState == window.STATE_NORMAL) {
    window.maximize();
  } else {
    window.restore();
  }
}
