/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Target } from "chrome://remote/content/cdp/targets/Target.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MainProcessSession:
    "chrome://remote/content/cdp/sessions/MainProcessSession.sys.mjs",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.sys.mjs",
});

/**
 * The main process Target.
 *
 * Matches BrowserDevToolsAgentHost from chromium, and only support a couple of Domains:
 * https://cs.chromium.org/chromium/src/content/browser/devtools/browser_devtools_agent_host.cc?dr=CSs&g=0&l=80-91
 */
export class MainProcessTarget extends Target {
  /*
   * @param TargetList targetList
   */
  constructor(targetList) {
    super(targetList, lazy.MainProcessSession);

    this.type = "browser";

    // Define the HTTP path to query this target
    this.path = `/devtools/browser/${this.id}`;
  }

  get wsDebuggerURL() {
    const { host, port } = lazy.RemoteAgent;
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
