/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1924500 - Fix www.tiktok.com captcha slider
 * WebCompat issue #142709 - https://webcompat.com/issues/142709
 *
 * They are relying on drag events having clientX coordinates, which are
 * always zero on Firefox. A work-around is to listen for mousemove and
 * dragover events to capture the clientX value from them, and then
 * pass the value along when they reads a DragEvent.clientX.
 */

/* globals exportFunction */

(function () {
  let lastClientX = 0;

  const win = window.wrappedJSObject;
  Object.defineProperty(win.DragEvent.prototype, "clientX", {
    get: exportFunction(function () {
      return lastClientX;
    }, window),
    set: exportFunction(function () {}, window),
    configurable: true,
  });

  function setLastClientX(evt) {
    lastClientX = evt.clientX;
  }

  const captchaOverlayQuery = ".TUXModal-overlay";

  function activate() {
    document.documentElement.addEventListener("mousemove", setLastClientX);
    document.documentElement.addEventListener("dragover", setLastClientX);
  }

  function deactivate() {
    lastClientX = 0;
    document.documentElement.removeEventListener("mousemove", setLastClientX);
    document.documentElement.removeEventListener("dragover", setLastClientX);
  }

  const captchaObserver = new MutationObserver(mutations => {
    for (let { addedNodes, removedNodes } of mutations) {
      for (const node of addedNodes) {
        try {
          if (node.matches(captchaOverlayQuery)) {
            activate();
          }
        } catch (_) {}
      }
      for (const node of removedNodes) {
        try {
          if (node.matches(captchaOverlayQuery)) {
            deactivate();
          }
        } catch (_) {}
      }
    }
  });

  captchaObserver.observe(document.documentElement, {
    childList: true,
    subtree: true,
  });
})();
