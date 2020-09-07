/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "UUIDGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);
const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);
const { TabManager } = ChromeUtils.import(
  "chrome://remote/content/TabManager.jsm"
);
const { TabSession } = ChromeUtils.import(
  "chrome://remote/content/sessions/TabSession.jsm"
);
const { WindowManager } = ChromeUtils.import(
  "chrome://remote/content/WindowManager.jsm"
);

let browserContextIds = 1;

class Target extends Domain {
  constructor(session) {
    super(session);

    this.onTargetCreated = this.onTargetCreated.bind(this);
    this.onTargetDestroyed = this.onTargetDestroyed.bind(this);
  }

  getBrowserContexts() {
    return {
      browserContextIds: [],
    };
  }

  createBrowserContext() {
    const identity = ContextualIdentityService.create(
      "remote-agent-" + browserContextIds++
    );
    return { browserContextId: identity.userContextId };
  }

  disposeBrowserContext({ browserContextId }) {
    ContextualIdentityService.remove(browserContextId);
    ContextualIdentityService.closeContainerTabs(browserContextId);
  }

  setDiscoverTargets({ discover }) {
    const { targets } = this.session.target;
    if (discover) {
      targets.on("target-created", this.onTargetCreated);
      targets.on("target-destroyed", this.onTargetDestroyed);
    } else {
      targets.off("target-created", this.onTargetCreated);
      targets.off("target-destroyed", this.onTargetDestroyed);
    }
    for (const target of targets) {
      this.onTargetCreated("target-created", target);
    }
  }

  onTargetCreated(eventName, target) {
    this.emit("Target.targetCreated", {
      targetInfo: {
        browserContextId: target.browserContextId,
        targetId: target.id,
        type: target.type,
        url: target.url,
      },
    });
  }

  onTargetDestroyed(eventName, target) {
    this.emit("Target.targetDestroyed", {
      targetId: target.id,
    });
  }

  async createTarget({ browserContextId }) {
    const { targets } = this.session.target;
    const onTarget = targets.once("target-created");
    const tab = TabManager.addTab({ userContextId: browserContextId });
    const target = await onTarget;
    if (tab.linkedBrowser != target.browser) {
      throw new Error(
        "Unexpected tab opened: " + tab.linkedBrowser.currentURI.spec
      );
    }
    return { targetId: target.id };
  }

  closeTarget({ targetId }) {
    const { targets } = this.session.target;
    const target = targets.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    TabManager.removeTab(target.tab);
  }

  async activateTarget({ targetId }) {
    const { targets, window } = this.session.target;
    const target = targets.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    // Focus the window, and select the corresponding tab
    await WindowManager.focus(window);
    TabManager.selectTab(target.tab);
  }

  attachToTarget({ targetId }) {
    const { targets } = this.session.target;
    const target = targets.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    const tabSession = new TabSession(
      this.session.connection,
      target,
      UUIDGen.generateUUID()
        .toString()
        .slice(1, -1)
    );
    this.session.connection.registerSession(tabSession);
    this.emit("Target.attachedToTarget", {
      targetInfo: {
        type: "page",
      },
      sessionId: tabSession.id,
    });

    return {
      sessionId: tabSession.id,
    };
  }

  setAutoAttach() {}

  sendMessageToTarget({ sessionId, message }) {
    const { connection } = this.session;
    connection.sendMessageToTarget(sessionId, message);
  }
}
