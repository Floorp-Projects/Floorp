/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gCPS",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gCaptivePortalEnabled",
  "network.captive-portal-service.enabled",
  false
);

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);

var { getSettingsAPI } = ExtensionPreferencesManager;

const CAPTIVE_URL_PREF = "captivedetect.canonicalURL";

var { ExtensionError } = ExtensionUtils;

this.captivePortal = class extends ExtensionAPI {
  checkCaptivePortalEnabled() {
    if (!gCaptivePortalEnabled) {
      throw new ExtensionError("Captive Portal detection is not enabled");
    }
  }

  nameForCPSState(state) {
    switch (state) {
      case gCPS.UNKNOWN:
        return "unknown";
      case gCPS.NOT_CAPTIVE:
        return "not_captive";
      case gCPS.UNLOCKED_PORTAL:
        return "unlocked_portal";
      case gCPS.LOCKED_PORTAL:
        return "locked_portal";
      default:
        return "unknown";
    }
  }

  PERSISTENT_EVENTS = {
    onStateChanged: fire => {
      this.checkCaptivePortalEnabled();

      let observer = (subject, topic) => {
        fire.async({ state: this.nameForCPSState(gCPS.state) });
      };

      Services.obs.addObserver(
        observer,
        "ipc:network:captive-portal-set-state"
      );
      return {
        unregister: () => {
          Services.obs.removeObserver(
            observer,
            "ipc:network:captive-portal-set-state"
          );
        },
        convert(_fire, context) {
          fire = _fire;
        },
      };
    },
    onConnectivityAvailable: fire => {
      this.checkCaptivePortalEnabled();

      let observer = (subject, topic, data) => {
        fire.async({ status: data });
      };

      Services.obs.addObserver(observer, "network:captive-portal-connectivity");
      return {
        unregister: () => {
          Services.obs.removeObserver(
            observer,
            "network:captive-portal-connectivity"
          );
        },
        convert(_fire, context) {
          fire = _fire;
        },
      };
    },
    "captiveURL.onChange": fire => {
      let listener = (text, id) => {
        fire.async({
          levelOfControl: "not_controllable",
          value: Services.prefs.getStringPref(CAPTIVE_URL_PREF),
        });
      };
      Services.prefs.addObserver(CAPTIVE_URL_PREF, listener);
      return {
        unregister: () => {
          Services.prefs.removeObserver(CAPTIVE_URL_PREF, listener);
        },
        convert(_fire, context) {
          fire = _fire;
        },
      };
    },
  };

  primeListener(extension, event, fire) {
    if (Object.hasOwn(this.PERSISTENT_EVENTS, event)) {
      return this.PERSISTENT_EVENTS[event](fire);
    }
  }

  getAPI(context) {
    let self = this;
    return {
      captivePortal: {
        getState() {
          self.checkCaptivePortalEnabled();
          return self.nameForCPSState(gCPS.state);
        },
        getLastChecked() {
          self.checkCaptivePortalEnabled();
          return gCPS.lastChecked;
        },
        onStateChanged: new EventManager({
          context,
          module: "captivePortal",
          event: "onStateChanged",
          register: fire => {
            return self.PERSISTENT_EVENTS.onStateChanged(fire).unregister;
          },
        }).api(),
        onConnectivityAvailable: new EventManager({
          context,
          module: "captivePortal",
          event: "onConnectivityAvailable",
          register: fire => {
            return self.PERSISTENT_EVENTS.onConnectivityAvailable(fire)
              .unregister;
          },
        }).api(),
        canonicalURL: getSettingsAPI({
          context,
          name: "captiveURL",
          callback() {
            return Services.prefs.getStringPref(CAPTIVE_URL_PREF);
          },
          readOnly: true,
          onChange: new ExtensionCommon.EventManager({
            context,
            module: "captivePortal",
            event: "captiveURL.onChange",
            register: fire => {
              return self.PERSISTENT_EVENTS["captiveURL.onChange"](fire)
                .unregister;
            },
          }).api(),
        }),
      },
    };
  }
};
