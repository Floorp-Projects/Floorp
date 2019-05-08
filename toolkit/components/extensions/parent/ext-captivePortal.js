/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "gCPS",
                                   "@mozilla.org/network/captive-portal-service;1",
                                   "nsICaptivePortalService");

XPCOMUtils.defineLazyPreferenceGetter(this, "gCaptivePortalEnabled",
                                      "network.captive-portal-service.enabled",
                                      false);

function nameForCPSState(state) {
  switch (state) {
    case gCPS.UNKNOWN: return "unknown";
    case gCPS.NOT_CAPTIVE: return "not_captive";
    case gCPS.UNLOCKED_PORTAL: return "unlocked_portal";
    case gCPS.LOCKED_PORTAL: return "locked_portal";
    default: return "unknown";
  }
}

var {
  ExtensionError,
} = ExtensionUtils;

this.captivePortal = class extends ExtensionAPI {
  getAPI(context) {
    function checkEnabled() {
      if (!gCaptivePortalEnabled) {
        throw new ExtensionError("Captive Portal detection is not enabled");
      }
    }

    return {
      captivePortal: {
        getState() {
          checkEnabled();
          return nameForCPSState(gCPS.state);
        },
        getLastChecked() {
          checkEnabled();
          return gCPS.lastChecked;
        },
        onStateChanged: new EventManager({
          context,
          name: "captivePortal.onStateChanged",
          register: fire => {
            checkEnabled();

            let observer = (subject, topic) => {
              fire.async({state: nameForCPSState(gCPS.state)});
            };

            Services.obs.addObserver(observer, "ipc:network:captive-portal-set-state");
            return () => {
              Services.obs.removeObserver(observer, "ipc:network:captive-portal-set-state");
            };
          },
        }).api(),
        onConnectivityAvailable: new EventManager({
          context,
          name: "captivePortal.onConnectivityAvailable",
          register: fire => {
            checkEnabled();

            let observer = (subject, topic, data) => {
              fire.async({status: data});
            };

            Services.obs.addObserver(observer, "network:captive-portal-connectivity");
            return () => {
              Services.obs.removeObserver(observer, "network:captive-portal-connectivity");
            };
          },
        }).api(),
      },
    };
  }
};
