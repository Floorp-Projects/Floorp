/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LogChild"];

const {RemoteAgentActorChild} = ChromeUtils.import("chrome://remote/content/Actor.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {WindowManager} = ChromeUtils.import("chrome://remote/content/WindowManager.jsm");

class LogChild extends RemoteAgentActorChild {
  observe(subject, topic) {
    const event = subject.wrappedJSObject;

    if (this.isEventRelevant(event)) {
      const entry = {
        source: "javascript",
        level: reduceLevel(event.level),
        text: event.arguments.join(" "),
        timestamp: event.timeStamp,
        url: event.fileName,
        lineNumber: event.lineNumber,
      };
      this.sendAsyncMessage("RemoteAgent:Log:OnConsoleAPIEvent", entry);
    }
  }

  isEventRelevant({innerID}) {
    const eventWin = Services.wm.getCurrentInnerWindowWithId(innerID);
    if (!eventWin || !WindowManager.isWindowIncluded(this.content, eventWin)) {
      return false;
    }
    return true;
  }
};

function reduceLevel(level) {
  switch (level) {
  case "exception":
    return "error";
  case "warn":
    return "warning";
  case "debug":
    return "verbose";
  case "log":
  default:
    return "info";
  }
}
