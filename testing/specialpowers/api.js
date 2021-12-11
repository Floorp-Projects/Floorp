/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ExtensionAPI, Services, XPCOMUtils */

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

this.specialpowers = class extends ExtensionAPI {
  onStartup() {
    let uri = Services.io.newURI("content/", null, this.extension.rootURI);
    resProto.setSubstitutionWithFlags(
      "specialpowers",
      uri,
      resProto.ALLOW_CONTENT_ACCESS
    );

    // Register special testing modules.
    Components.manager
      .QueryInterface(Ci.nsIComponentRegistrar)
      .autoRegister(FileUtils.getFile("ProfD", ["tests.manifest"]));

    ChromeUtils.registerWindowActor("SpecialPowers", {
      allFrames: true,
      includeChrome: true,
      child: {
        moduleURI: "resource://specialpowers/SpecialPowersChild.jsm",
        observers: [
          "chrome-document-global-created",
          "content-document-global-created",
        ],
      },
      parent: {
        moduleURI: "resource://specialpowers/SpecialPowersParent.jsm",
      },
    });

    ChromeUtils.registerWindowActor("AppTestDelegate", {
      parent: {
        moduleURI: "resource://specialpowers/AppTestDelegateParent.jsm",
      },
      child: {
        moduleURI: "resource://specialpowers/AppTestDelegateChild.jsm",
        events: {
          DOMContentLoaded: { capture: true },
          load: { capture: true },
        },
      },
      allFrames: true,
      includeChrome: true,
    });
  }

  onShutdown() {
    ChromeUtils.unregisterWindowActor("SpecialPowers");
    ChromeUtils.unregisterWindowActor("AppTestDelegate");
    resProto.setSubstitution("specialpowers", null);
  }
};
