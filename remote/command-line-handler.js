/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Stopgap module until we can land bug 1536862 and remove this temporary file

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "RemoteAgent",
    "chrome://remote/content/RemoteAgent.jsm");

const RemoteAgentFactory = {
  createInstance(outer, iid) {
    if (outer) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    // we can't use Preferences.jsm before first paint
    if (!Services.prefs.getBoolPref("remote.enabled")) {
      return {};
    }

    return RemoteAgent.QueryInterface(iid);
  },
};

function RemoteAgentComponent() {}

RemoteAgentComponent.prototype = {
    classDescription: "Remote Agent",
    classID: Components.ID("{8f685a9d-8181-46d6-a71d-869289099c6d}"),
    contractID: "@mozilla.org/remote/agent",
    _xpcom_factory: RemoteAgentFactory,  /* eslint-disable-line */
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteAgentComponent]);
