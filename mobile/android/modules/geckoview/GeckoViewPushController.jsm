/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPushController"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PushNotifier",
  "@mozilla.org/push/Notifier;1",
  "nsIPushNotifier"
);

// eslint-disable-next-line no-unused-vars
const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPushController");

function createScopeAndPrincipal(scopeAndAttrs) {
  const [scope, attrs] = scopeAndAttrs.split("^");
  const uri = Services.io.newURI(scope);

  return [
    scope,
    Services.scriptSecurityManager.createContentPrincipal(
      uri,
      ChromeUtils.createOriginAttributesFromOrigin(attrs)
    ),
  ];
}

const GeckoViewPushController = {
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:PushEvent": {
        const {
          subscription: { scope },
          data,
        } = aData;

        const [url, principal] = createScopeAndPrincipal(scope);

        if (!data) {
          PushNotifier.notifyPush(url, principal);
          return;
        }

        const payload = new Uint8Array(
          ChromeUtils.base64URLDecode(data, { padding: "ignore" })
        );

        PushNotifier.notifyPushWithData(url, principal, "", payload);
        break;
      }
      case "GeckoView:PushSubscriptionChanged": {
        const {
          subscription: { scope },
        } = aData;

        const [url, principal] = createScopeAndPrincipal(scope);

        PushNotifier.notifySubscriptionChange(url, principal);
        break;
      }
    }
  },
};
