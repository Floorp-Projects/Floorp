/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

const WEBEXTENSION_ID = "tabswitch-talos@mozilla.org";
const ABOUT_PAGE_NAME = "tabswitch";
const Registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const UUID = "0f459ab4-b4ba-4741-ac89-ee47dea07adb";
const ABOUT_PATH_PATH = "content/test.html";

const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

let factory;

export class TalosTabSwitchChild extends RemotePageChild {
  actorCreated() {
    // Ignore about:blank pages that can get here.
    if (!String(this.document.location).startsWith("about:tabswitch")) {
      return;
    }

    // If an error occurs, it was probably already added by an earlier test run.
    try {
      this.addPage("about:tabswitch", {
        RPMSendQuery: ["tabswitch-do-test"],
      });
    } catch {}

    super.actorCreated();
  }

  handleEvent(event) {}

  receiveMessage(message) {
    if (message.name == "Tabswitch:Teardown") {
      this.teardown();
    } else if (message.name == "GarbageCollect") {
      this.contentWindow.windowUtils.garbageCollect();
    }
  }

  teardown() {
    Registrar.unregisterFactory(Components.ID(UUID), this._factory);
    factory = null;
  }
}

function setupTabSwitch() {
  let extensionPolicy = WebExtensionPolicy.getByID(WEBEXTENSION_ID);
  let aboutPageURI = extensionPolicy.getURL(ABOUT_PATH_PATH);

  class TabSwitchAboutModule {
    constructor() {
      this.QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
    }
    newChannel(aURI, aLoadInfo) {
      let uri = Services.io.newURI(aboutPageURI);
      let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
      chan.originalURI = aURI;
      return chan;
    }
    getURIFlags(aURI) {
      return (
        Ci.nsIAboutModule.ALLOW_SCRIPT |
        Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD
      );
    }
  }

  factory = ComponentUtils.generateSingletonFactory(TabSwitchAboutModule);

  Registrar.registerFactory(
    Components.ID(UUID),
    "",
    `@mozilla.org/network/protocol/about;1?what=${ABOUT_PAGE_NAME}`,
    factory
  );
}

setupTabSwitch();
