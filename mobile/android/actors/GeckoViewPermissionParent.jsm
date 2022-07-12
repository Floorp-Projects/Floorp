/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewPermissionParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);

class GeckoViewPermissionParent extends GeckoViewActorParent {
  _appPermissions = {};

  async getAppPermissions(aPermissions) {
    const perms = aPermissions.filter(perm => !this._appPermissions[perm]);
    if (!perms.length) {
      return Promise.resolve(/* granted */ true);
    }

    const granted = await this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:AndroidPermission",
      perms,
    });

    if (granted) {
      for (const perm of perms) {
        this._appPermissions[perm] = true;
      }
    }

    return granted;
  }

  addCameraPermission() {
    const principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      this.browsingContext.top.currentWindowGlobal.documentPrincipal.origin
    );

    // Although the lifetime is "session" it will be removed upon
    // use so it's more of a one-shot.
    Services.perms.addFromPrincipal(
      principal,
      "MediaManagerVideo",
      Services.perms.ALLOW_ACTION,
      Services.perms.EXPIRE_SESSION
    );

    return null;
  }

  receiveMessage(aMessage) {
    debug`receiveMessage ${aMessage.name}`;

    switch (aMessage.name) {
      case "GetAppPermissions": {
        return this.getAppPermissions(aMessage.data);
      }
      case "AddCameraPermission": {
        return this.addCameraPermission();
      }
    }

    return super.receiveMessage(aMessage);
  }
}

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPermissionParent");
