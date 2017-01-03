/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

const bundle = Services.strings.createBundle(
  "chrome://global/locale/aboutServiceWorkers.properties");

const brandBundle = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties");

var gSWM;
var gSWCount = 0;

function init() {
  let enabled = Services.prefs.getBoolPref("dom.serviceWorkers.enabled");
  if (!enabled) {
    let div = document.getElementById("warning_not_enabled");
    div.classList.add("active");
    return;
  }

  gSWM = Cc["@mozilla.org/serviceworkers/manager;1"]
           .getService(Ci.nsIServiceWorkerManager);
  if (!gSWM) {
    dump("AboutServiceWorkers: Failed to get the ServiceWorkerManager service!\n");
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
    ps = Cc["@mozilla.org/push/Service;1"]
           .getService(Ci.nsIPushService);
  } catch (e) {
    dump("Could not acquire PushService\n");
  }

  for (let i = 0; i < length; ++i) {
    let info = data.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
    if (!info) {
      dump("AboutServiceWorkers: Invalid nsIServiceWorkerRegistrationInfo interface.\n");
      continue;
    }

    display(info, ps);
  }
}

function display(info, pushService) {
  let parent = document.getElementById("serviceworkers");

  let div = document.createElement('div');
  parent.appendChild(div);

  let title = document.createElement('h2');
  let titleStr = bundle.formatStringFromName('title', [info.principal.origin], 1);
  title.appendChild(document.createTextNode(titleStr));
  div.appendChild(title);

  if (info.principal.appId) {
    let b2gtitle = document.createElement('h3');
    let trueFalse = bundle.GetStringFromName(info.principal.isInIsolatedMozBrowserElement ? 'true' : 'false');

    let b2gtitleStr =
      bundle.formatStringFromName('b2gtitle', [ brandBundle.getString("brandShortName"),
                                                info.principal.appId,
                                                trueFalse], 2);
    b2gtitle.appendChild(document.createTextNode(b2gtitleStr));
    div.appendChild(b2gtitle);
  }

  let list = document.createElement('ul');
  div.appendChild(list);

  function createItem(title, value, makeLink) {
    let item = document.createElement('li');
    list.appendChild(item);

    let bold = document.createElement('strong');
    bold.appendChild(document.createTextNode(title + " "));
    item.appendChild(bold);

    let textNode = document.createTextNode(value);

    if (makeLink) {
      let link = document.createElement("a");
      link.href = value;
      link.target = "_blank";
      link.appendChild(textNode);
      item.appendChild(link);
    } else {
      item.appendChild(textNode);
    }

    return textNode;
  }

  createItem(bundle.GetStringFromName('scope'), info.scope);
  createItem(bundle.GetStringFromName('scriptSpec'), info.scriptSpec, true);
  let currentWorkerURL = info.activeWorker ? info.activeWorker.scriptSpec : "";
  createItem(bundle.GetStringFromName('currentWorkerURL'), currentWorkerURL, true);
  let activeCacheName = info.activeWorker ? info.activeWorker.cacheName : "";
  createItem(bundle.GetStringFromName('activeCacheName'), activeCacheName);
  let waitingCacheName = info.waitingWorker ? info.waitingWorker.cacheName : "";
  createItem(bundle.GetStringFromName('waitingCacheName'), waitingCacheName);

  let pushItem = createItem(bundle.GetStringFromName('pushEndpoint'), bundle.GetStringFromName('waiting'));
  if (pushService) {
    pushService.getSubscription(info.scope, info.principal, (status, pushRecord) => {
      if (Components.isSuccessCode(status)) {
        pushItem.data = JSON.stringify(pushRecord);
      } else {
        dump("about:serviceworkers - retrieving push registration failed\n");
      }
    });
  }

  let updateButton = document.createElement("button");
  updateButton.appendChild(document.createTextNode(bundle.GetStringFromName('update')));
  updateButton.onclick = function() {
    gSWM.propagateSoftUpdate(info.principal.originAttributes, info.scope);
  };
  div.appendChild(updateButton);

  let unregisterButton = document.createElement("button");
  unregisterButton.appendChild(document.createTextNode(bundle.GetStringFromName('unregister')));
  div.appendChild(unregisterButton);

  let loadingMessage = document.createElement('span');
  loadingMessage.appendChild(document.createTextNode(bundle.GetStringFromName('waiting')));
  loadingMessage.classList.add('inactive');
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

      unregisterFailed() {
        alert(bundle.GetStringFromName('unregisterError'));
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIServiceWorkerUnregisterCallback])
    };

    loadingMessage.classList.remove('inactive');
    gSWM.propagateUnregister(info.principal, cb, info.scope);
  };

  let sep = document.createElement('hr');
  div.appendChild(sep);

  ++gSWCount;
}

window.addEventListener("DOMContentLoaded", function load() {
  window.removeEventListener("DOMContentLoaded", load);
  init();
});
