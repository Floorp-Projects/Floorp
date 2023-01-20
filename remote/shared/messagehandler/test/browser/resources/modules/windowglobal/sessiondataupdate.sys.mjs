/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class SessionDataUpdateModule extends Module {
  #sessionDataUpdates;

  constructor(messageHandler) {
    super(messageHandler);
    this.#sessionDataUpdates = [];
  }

  destroy() {}

  /**
   * Commands
   */

  _applySessionData(params) {
    const filteredSessionData = params.sessionData.filter(item =>
      this.messageHandler.matchesContext(item.contextDescriptor)
    );
    this.#sessionDataUpdates.push(filteredSessionData);
  }

  getSessionDataUpdates() {
    return this.#sessionDataUpdates;
  }
}

export const sessiondataupdate = SessionDataUpdateModule;
