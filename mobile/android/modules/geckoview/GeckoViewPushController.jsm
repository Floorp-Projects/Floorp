/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPushController"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "PushNotifier",
  "@mozilla.org/push/Notifier;1",
  "nsIPushNotifier"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPushController");

function createScopeAndPrincipal(scopeAndAttrs) {
  const principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    scopeAndAttrs
  );
  const scope = principal.URI.spec;

  return [scope, principal];
}

const GeckoViewPushController = {
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:PushEvent": {
        const { scope, data } = aData;

        const [url, principal] = createScopeAndPrincipal(scope);

        if (
          Services.perms.testPermissionFromPrincipal(
            principal,
            "desktop-notification"
          ) != Services.perms.ALLOW_ACTION
        ) {
          return;
        }

        if (!data) {
          lazy.PushNotifier.notifyPush(url, principal, "");
          return;
        }

        const payload = new Uint8Array(
          ChromeUtils.base64URLDecode(data, { padding: "ignore" })
        );

        lazy.PushNotifier.notifyPushWithData(url, principal, "", payload);
        break;
      }
      case "GeckoView:PushSubscriptionChanged": {
        const { scope } = aData;

        const [url, principal] = createScopeAndPrincipal(scope);

        lazy.PushNotifier.notifySubscriptionChange(url, principal);
        break;
      }
    }
  },
};
