/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, Services, XPCOMUtils */

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

const ResourceSubstitution = "webcompat";
const ProcessScriptURL = "resource://webcompat/aboutPageProcessScript.js";

this.aboutPage = class extends ExtensionAPI {
  onStartup() {
    const {rootURI} = this.extension;

    resProto.setSubstitution(ResourceSubstitution,
                             Services.io.newURI("chrome/res/", null, rootURI));

    Services.ppmm.loadProcessScript(ProcessScriptURL, true);
  }

  onShutdown() {
    resProto.setSubstitution(ResourceSubstitution, null);

    Services.ppmm.removeDelayedProcessScript(ProcessScriptURL);
  }
};
