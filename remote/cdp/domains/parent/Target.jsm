/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Target"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",

  Domain: "chrome://remote/content/cdp/domains/Domain.jsm",
  MainProcessTarget:
    "chrome://remote/content/cdp/targets/MainProcessTarget.jsm",
  TabManager: "chrome://remote/content/shared/TabManager.jsm",
  TabSession: "chrome://remote/content/cdp/sessions/TabSession.jsm",
  WindowManager: "chrome://remote/content/shared/WindowManager.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UUIDGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

let browserContextIds = 1;

class Target extends Domain {
  constructor(session) {
    super(session);

    this._onTargetCreated = this._onTargetCreated.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
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

  disposeBrowserContext(options = {}) {
    const { browserContextId } = options;

    ContextualIdentityService.remove(browserContextId);
    ContextualIdentityService.closeContainerTabs(browserContextId);
  }

  getTargets() {
    const { targetList } = this.session.target;

    const targetInfos = [];
    for (const target of targetList) {
      if (target instanceof MainProcessTarget) {
        continue;
      }

      targetInfos.push(this._getTargetInfo(target));
    }

    return { targetInfos };
  }

  setDiscoverTargets(options = {}) {
    const { discover } = options;
    const { targetList } = this.session.target;
    if (discover) {
      targetList.on("target-created", this._onTargetCreated);
      targetList.on("target-destroyed", this._onTargetDestroyed);
    } else {
      targetList.off("target-created", this._onTargetCreated);
      targetList.off("target-destroyed", this._onTargetDestroyed);
    }
    for (const target of targetList) {
      this._onTargetCreated("target-created", target);
    }
  }

  async createTarget(options = {}) {
    const { browserContextId } = options;
    const { targetList } = this.session.target;
    const onTarget = targetList.once("target-created");
    const tab = TabManager.addTab({ userContextId: browserContextId });
    const target = await onTarget;
    if (tab.linkedBrowser != target.browser) {
      throw new Error(
        "Unexpected tab opened: " + tab.linkedBrowser.currentURI.spec
      );
    }
    return { targetId: target.id };
  }

  closeTarget(options = {}) {
    const { targetId } = options;
    const { targetList } = this.session.target;
    const target = targetList.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    TabManager.removeTab(target.tab);
  }

  async activateTarget(options = {}) {
    const { targetId } = options;
    const { targetList, window } = this.session.target;
    const target = targetList.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    // Focus the window, and select the corresponding tab
    await WindowManager.focus(window);
    TabManager.selectTab(target.tab);
  }

  attachToTarget(options = {}) {
    const { targetId } = options;
    const { targetList } = this.session.target;
    const target = targetList.getById(targetId);

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

    this._emitAttachedToTarget(target, tabSession);

    return {
      sessionId: tabSession.id,
    };
  }

  setAutoAttach() {}

  sendMessageToTarget(options = {}) {
    const { sessionId, message } = options;
    const { connection } = this.session;
    connection.sendMessageToTarget(sessionId, message);
  }

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  _emitAttachedToTarget(target, tabSession) {
    const targetInfo = this._getTargetInfo(target);
    this.emit("Target.attachedToTarget", {
      targetInfo,
      sessionId: tabSession.id,
      waitingForDebugger: false,
    });
  }

  _getTargetInfo(target) {
    return {
      targetId: target.id,
      type: target.type,
      title: target.title,
      url: target.url,
      // TODO: Correctly determine if target is attached (bug 1680780)
      attached: target.id == this.session.target.id,
      browserContextId: target.browserContextId,
    };
  }

  _onTargetCreated(eventName, target) {
    const targetInfo = this._getTargetInfo(target);
    this.emit("Target.targetCreated", { targetInfo });
  }

  _onTargetDestroyed(eventName, target) {
    this.emit("Target.targetDestroyed", {
      targetId: target.id,
    });
  }
}
