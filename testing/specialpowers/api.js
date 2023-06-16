/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ExtensionAPI, Services, XPCOMUtils */

ChromeUtils.defineESModuleGetters(this, {
  SpecialPowersParent: "resource://testing-common/SpecialPowersParent.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

this.specialpowers = class extends ExtensionAPI {
  onStartup() {
    // Register special testing modules.
    let manifest = Services.dirsvc.get("ProfD", Ci.nsIFile);
    manifest.append("tests.manifest");
    Components.manager
      .QueryInterface(Ci.nsIComponentRegistrar)
      .autoRegister(manifest);

    {
      let uri = Services.io.newURI("content/", null, this.extension.rootURI);
      resProto.setSubstitutionWithFlags(
        "specialpowers",
        uri,
        resProto.ALLOW_CONTENT_ACCESS
      );
    }

    if (!resProto.hasSubstitution("testing-common")) {
      let uri = Services.io.newURI("modules/", null, this.extension.rootURI);
      resProto.setSubstitution(
        "testing-common",
        uri,
        resProto.ALLOW_CONTENT_ACCESS
      );
    }

    SpecialPowersParent.registerActor();

    ChromeUtils.registerWindowActor("AppTestDelegate", {
      parent: {
        esModuleURI: "resource://specialpowers/AppTestDelegateParent.sys.mjs",
      },
      child: {
        esModuleURI: "resource://specialpowers/AppTestDelegateChild.sys.mjs",
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
    SpecialPowersParent.unregisterActor();
    ChromeUtils.unregisterWindowActor("AppTestDelegate");
    resProto.setSubstitution("specialpowers", null);
  }
};
