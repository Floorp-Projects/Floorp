/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ExtensionAPI */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

this.specialpowers = class extends ExtensionAPI {
  onStartup() {
    let uri = Services.io.newURI("content/", null, this.extension.rootURI);
    resProto.setSubstitutionWithFlags("specialpowers", uri,
                                      resProto.ALLOW_CONTENT_ACCESS);

    ChromeUtils.import("resource://specialpowers/SpecialPowersObserver.jsm");
    this.observer = new SpecialPowersObserver();
    this.observer.init();
  }

  onShutdown() {
    this.observer.uninit();
    this.observer = null;
    resProto.setSubstitution("specialpowers", null);
  }
};
