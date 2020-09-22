/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, XPCOMUtils */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

this.tabExtras = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);
    this._registerActorModule();
  }

  getAPI(context) {
    const { tabManager } = context.extension;
    return {
      tabExtras: {
        async getWebcompatInfo(tabId) {
          const { browsingContext } = tabManager.get(tabId).browser;
          const actors = gatherActors("ReportSiteIssueHelper", browsingContext);
          const promises = actors.map(actor => actor.sendQuery("GetLog"));
          const logs = await Promise.all(promises);
          const info = await actors[0].sendQuery("GetBlockingStatus");
          info.hasMixedActiveContentBlocked = !!(
            browsingContext.secureBrowserUI.state &
            Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_ACTIVE_CONTENT
          );
          info.hasMixedDisplayContentBlocked = !!(
            browsingContext.secureBrowserUI.state &
            Ci.nsIWebProgressListener.STATE_BLOCKED_MIXED_DISPLAY_CONTENT
          );
          info.log = logs
            .flat()
            .sort((a, b) => a.timeStamp - b.timeStamp)
            .map(m => m.message);
          return info;
        },
      },
    };
  }

  onShutdown(isAppShutdown) {
    this._unregisterActorModule();
  }

  _registerActorModule() {
    resProto.setSubstitution(
      "report-site-issue",
      Services.io.newURI(
        "experimentalAPIs/actors/",
        null,
        this.extension.rootURI
      )
    );
    ChromeUtils.registerWindowActor("ReportSiteIssueHelper", {
      child: {
        moduleURI: "resource://report-site-issue/tabExtrasActor.jsm",
      },
      allFrames: true,
    });
  }

  _unregisterActorModule() {
    ChromeUtils.unregisterWindowActor("ReportSiteIssueHelper");
    resProto.setSubstitution("report-site-issue", null);
  }
};

function getActorForBrowsingContext(name, browsingContext) {
  const windowGlobal = browsingContext.currentWindowGlobal;
  return windowGlobal ? windowGlobal.getActor(name) : null;
}

function gatherActors(name, browsingContext) {
  const list = [];

  const actor = getActorForBrowsingContext(name, browsingContext);
  if (actor) {
    list.push(actor);
  }

  for (const child of browsingContext.children) {
    list.push(...gatherActors(name, child));
  }

  return list;
}
