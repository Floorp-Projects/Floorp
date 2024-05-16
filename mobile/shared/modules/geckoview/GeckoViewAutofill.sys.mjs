/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

class Autofill {
  constructor(sessionId, eventDispatcher) {
    this.eventDispatcher = eventDispatcher;
    this.sessionId = sessionId;
  }

  start() {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:StartAutofill",
      sessionId: this.sessionId,
    });
  }

  add(node) {
    return this.eventDispatcher.sendRequestForResult({
      type: "GeckoView:AddAutofill",
      node,
    });
  }

  focus(node) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:OnAutofillFocus",
      node,
    });
  }

  update(node) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:UpdateAutofill",
      node,
    });
  }

  commit(node) {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:CommitAutofill",
      node,
    });
  }

  clear() {
    this.eventDispatcher.sendRequest({
      type: "GeckoView:ClearAutofill",
    });
  }
}

class AutofillManager {
  sessions = new Set();
  autofill = null;

  ensure(sessionId, eventDispatcher) {
    if (!this.sessions.has(sessionId)) {
      this.autofill = new Autofill(sessionId, eventDispatcher);
      this.sessions.add(sessionId);
      this.autofill.start();
    }
    // This could be called for an outdated session, in which case we will just
    // ignore the autofill call.
    if (sessionId !== this.autofill.sessionId) {
      return null;
    }
    return this.autofill;
  }

  get(sessionId) {
    if (!this.autofill || sessionId !== this.autofill.sessionId) {
      warn`Disregarding old session ${sessionId}`;
      // We disregard old sessions
      return null;
    }
    return this.autofill;
  }

  delete(sessionId) {
    this.sessions.delete(sessionId);
    if (!this.autofill || sessionId !== this.autofill.sessionId) {
      // this delete call might happen *after* the next session already
      // started, in that case, we can safely ignore this call.
      return;
    }
    this.autofill.clear();
    this.autofill = null;
  }
}

export var gAutofillManager = new AutofillManager();

const { debug, warn } = GeckoViewUtils.initLogging("Autofill");
