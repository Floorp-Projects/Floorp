/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var gSWM;
var gSWCount = 0;

function init() {
  let enabled = Services.prefs.getBoolPref("dom.serviceWorkers.enabled");
  if (!enabled) {
    let div = document.getElementById("warning_not_enabled");
    div.classList.add("active");
    return;
  }

  gSWM = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
    Ci.nsIServiceWorkerManager
  );
  if (!gSWM) {
    dump(
      "AboutServiceWorkers: Failed to get the ServiceWorkerManager service!\n"
    );
    return;
  }

  let data = gSWM.getAllRegistrations();
  if (!data) {
    dump("AboutServiceWorkers: Failed to retrieve the registrations.\n");
    return;
  }

  let length = data.length;
  if (!length) {
    let div = document.getElementById("warning_no_serviceworkers");
    div.classList.add("active");
    return;
  }

  let ps = undefined;
  try {
    ps = Cc["@mozilla.org/push/Service;1"].getService(Ci.nsIPushService);
  } catch (e) {
    dump("Could not acquire PushService\n");
  }

  for (let i = 0; i < length; ++i) {
    let info = data.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    if (!info) {
      dump(
        "AboutServiceWorkers: Invalid nsIServiceWorkerRegistrationInfo interface.\n"
      );
      continue;
    }

    display(info, ps);
  }
}

async function display(info, pushService) {
  let parent = document.getElementById("serviceworkers");

  let div = document.createElement("div");
  parent.appendChild(div);

  let title = document.createElement("h2");
  document.l10n.setAttributes(title, "origin-title", {
    originTitle: info.principal.origin,
  });
  div.appendChild(title);

  let list = document.createElement("ul");
  div.appendChild(list);

  function createItem(l10nId, value, makeLink) {
    let item = document.createElement("li");
    list.appendChild(item);
    let bold = document.createElement("strong");
    bold.setAttribute("data-l10n-name", "item-label");
    item.appendChild(bold);
    // Falsey values like "" are still valid values, so check exactly against
    // undefined for the cases where the caller did not provide any value.
    if (value === undefined) {
      document.l10n.setAttributes(item, l10nId);
    } else if (makeLink) {
      let link = document.createElement("a");
      link.setAttribute("target", "_blank");
      link.setAttribute("data-l10n-name", "link");
      link.setAttribute("href", value);
      item.appendChild(link);
      document.l10n.setAttributes(item, l10nId, { url: value });
    } else {
      document.l10n.setAttributes(item, l10nId, { name: value });
    }
    return item;
  }

  createItem("scope", info.scope);
  createItem("script-spec", info.scriptSpec, true);
  let currentWorkerURL = info.activeWorker ? info.activeWorker.scriptSpec : "";
  createItem("current-worker-url", currentWorkerURL, true);
  let activeCacheName = info.activeWorker ? info.activeWorker.cacheName : "";
  createItem("active-cache-name", activeCacheName);
  let waitingCacheName = info.waitingWorker ? info.waitingWorker.cacheName : "";
  createItem("waiting-cache-name", waitingCacheName);

  let pushItem = createItem("push-end-point-waiting");
  if (pushService) {
    pushService.getSubscription(
      info.scope,
      info.principal,
      (status, pushRecord) => {
        if (Components.isSuccessCode(status)) {
          document.l10n.setAttributes(pushItem, "push-end-point-result", {
            name: JSON.stringify(pushRecord),
          });
        } else {
          dump("about:serviceworkers - retrieving push registration failed\n");
        }
      }
    );
  }

  let unregisterButton = document.createElement("button");
  document.l10n.setAttributes(unregisterButton, "unregister-button");
  div.appendChild(unregisterButton);

  let loadingMessage = document.createElement("span");
  document.l10n.setAttributes(loadingMessage, "waiting");
  loadingMessage.classList.add("inactive");
  div.appendChild(loadingMessage);

  unregisterButton.onclick = function() {
    let cb = {
      unregisterSucceeded() {
        parent.removeChild(div);

        if (!--gSWCount) {
          let div = document.getElementById("warning_no_serviceworkers");
          div.classList.add("active");
        }
      },

      async unregisterFailed() {
        let [alertMsg] = await document.l10n.formatValues([
          { id: "unregister-error" },
        ]);
        alert(alertMsg);
      },

      QueryInterface: ChromeUtils.generateQI([
        "nsIServiceWorkerUnregisterCallback",
      ]),
    };

    loadingMessage.classList.remove("inactive");
    gSWM.propagateUnregister(info.principal, cb, info.scope);
  };

  let sep = document.createElement("hr");
  div.appendChild(sep);

  ++gSWCount;
}

window.addEventListener(
  "DOMContentLoaded",
  function() {
    init();
  },
  { once: true }
);
