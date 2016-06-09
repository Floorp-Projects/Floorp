/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Console.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "gFlyWebBundle", function() {
  return Services.strings.createBundle("chrome://flyweb/locale/flyweb.properties");
});

let discoveryManager = new FlyWebDiscoveryManager();

let discoveryCallback = {
  onDiscoveredServicesChanged(services) {
    if (!this.id) {
      return;
    }

    let list = document.getElementById("flyweb-list");
    while (list.firstChild) {
      list.firstChild.remove();
    }

    let template = document.getElementById("flyweb-item-template");

    for (let service of services) {
      let item = template.cloneNode(true);
      item.removeAttribute("id");

      item.setAttribute("data-service-id", service.serviceId);
      item.querySelector(".title").setAttribute("value", service.displayName);
      item.querySelector(".icon").src = "chrome://flyweb/content/icon-64.png";

      list.appendChild(item);
    }
  },
  start() {
    this.id = discoveryManager.startDiscovery(this);
  },
  stop() {
    discoveryManager.stopDiscovery(this.id);
    this.id = undefined;
  }
};

window.addEventListener("DOMContentLoaded", () => {
  let list = document.getElementById("flyweb-list");
  list.addEventListener("click", (evt) => {
    let serviceId = evt.target.closest("[data-service-id]").getAttribute("data-service-id");

    discoveryManager.pairWithService(serviceId, {
      pairingSucceeded(service) {
        window.open(service.uiUrl, "FlyWebWindow_" + serviceId);
      },

      pairingFailed(error) {
        console.error("FlyWeb failed to connect to service " + serviceId, error);
      }
    });
  });

  discoveryCallback.start();
});

window.addEventListener("unload", () => {
  discoveryCallback.stop();
});
