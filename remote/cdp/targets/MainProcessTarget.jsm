/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MainProcessTarget"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  MainProcessSession:
    "chrome://remote/content/cdp/sessions/MainProcessSession.jsm",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.jsm",
  Target: "chrome://remote/content/cdp/targets/Target.jsm",
});

/**
 * The main process Target.
 *
 * Matches BrowserDevToolsAgentHost from chromium, and only support a couple of Domains:
 * https://cs.chromium.org/chromium/src/content/browser/devtools/browser_devtools_agent_host.cc?dr=CSs&g=0&l=80-91
 */
class MainProcessTarget extends Target {
  /*
   * @param TargetList targetList
   */
  constructor(targetList) {
    super(targetList, MainProcessSession);

    this.type = "browser";

    // Define the HTTP path to query this target
    this.path = `/devtools/browser/${this.id}`;
  }

  get wsDebuggerURL() {
    const { host, port } = RemoteAgent;
    return `ws://${host}:${port}${this.path}`;
  }

  toString() {
    return `[object MainProcessTarget]`;
  }

  toJSON() {
    return {
      description: "Main process target",
      devtoolsFrontendUrl: "",
      faviconUrl: "",
      id: this.id,
      title: "Main process target",
      type: this.type,
      url: "",
      webSocketDebuggerUrl: this.wsDebuggerURL,
    };
  }
}
