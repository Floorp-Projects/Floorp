/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const {Domain} = ChromeUtils.import("chrome://remote/content/domains/Domain.jsm");
const {TabManager} = ChromeUtils.import("chrome://remote/content/WindowManager.jsm");
const {TabSession} = ChromeUtils.import("chrome://remote/content/sessions/TabSession.jsm");

let sessionIds = 1;

class Target extends Domain {
  constructor(session) {
    super(session);

    this.onTargetCreated = this.onTargetCreated.bind(this);
  }

  getBrowserContexts() {
    return {
      browserContextIds: [],
    };
  }

  setDiscoverTargets({ discover }) {
    const { targets } = this.session.target;
    if (discover) {
      targets.on("connect", this.onTargetCreated);
    } else {
      targets.off("connect", this.onTargetCreated);
    }
    for (const target of targets) {
      this.onTargetCreated("connect", target);
    }
  }

  onTargetCreated(eventName, target) {
    this.emit("Target.targetCreated", {
      targetInfo: {
        browserContextId: target.id,
        targetId: target.id,
        type: "page",
      },
    });
  }

  async createTarget() {
    const {targets} = this.session.target;
    const onTarget = targets.once("connect");
    const tab = TabManager.addTab();
    const target = await onTarget;
    if (tab.linkedBrowser != target.browser) {
      throw new Error("Unexpected tab opened: " + tab.linkedBrowser.currentURI.spec);
    }
    return {targetId: target.id};
  }

  attachToTarget({ targetId }) {
    const { targets } = this.session.target;
    const target = targets.getById(targetId);
    if (!target) {
      return new Error(`Unable to find target with id '${targetId}'`);
    }

    const session = new TabSession(this.session.connection, target, sessionIds++, this.session);
    this.emit("Target.attachedToTarget", {
      targetInfo: {
        type: "page",
      },
      sessionId: session.id,
    });

    return {
      sessionId: session.id,
    };
  }

  setAutoAttach() {}

  sendMessageToTarget({ sessionId, message }) {
    const { sessions } = this.session.connection;
    const session = sessions.get(sessionId);
    if (!session) {
      throw new Error(`Session '${sessionId}' doesn't exists.`);
    }
    message = JSON.parse(message);
    session.onMessage(message);
  }
}
