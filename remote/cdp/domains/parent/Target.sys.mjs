/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Domain } from "chrome://remote/content/cdp/domains/Domain.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",

  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  TabSession: "chrome://remote/content/cdp/sessions/TabSession.sys.mjs",
  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
});

let browserContextIds = 1;

// Default filter from CDP specification
const defaultFilter = [
  { type: "browser", exclude: true },
  { type: "tab", exclude: true },
  {},
];

export class Target extends Domain {
  #browserContextIds;
  #discoverTargetFilter;

  constructor(session) {
    super(session);

    this.#browserContextIds = new Set();

    this._onTargetCreated = this._onTargetCreated.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
  }

  getBrowserContexts() {
    const browserContextIds =
      lazy.ContextualIdentityService.getPublicUserContextIds().filter(id =>
        this.#browserContextIds.has(id)
      );

    return { browserContextIds };
  }

  createBrowserContext() {
    const identity = lazy.ContextualIdentityService.create(
      "remote-agent-" + browserContextIds++
    );

    this.#browserContextIds.add(identity.userContextId);
    return { browserContextId: identity.userContextId };
  }

  disposeBrowserContext(options = {}) {
    const { browserContextId } = options;

    lazy.ContextualIdentityService.remove(browserContextId);
    lazy.ContextualIdentityService.closeContainerTabs(browserContextId);

    this.#browserContextIds.delete(browserContextId);
  }

  getTargets(options = {}) {
    const { filter = defaultFilter } = options;
    const { targetList } = this.session.target;

    this._validateTargetFilter(filter);

    const targetInfos = [...targetList]
      .filter(target => this._filterIncludesTarget(target, filter))
      .map(target => this._getTargetInfo(target));

    return {
      targetInfos,
    };
  }

  setDiscoverTargets(options = {}) {
    const { discover, filter } = options;
    const { targetList } = this.session.target;

    if (typeof discover !== "boolean") {
      throw new TypeError("discover: boolean value expected");
    }

    if (discover === false && filter !== undefined) {
      throw new Error("filter: should not be present when discover is false");
    }

    // null filter should not be defaulted
    const targetFilter = filter === undefined ? defaultFilter : filter;
    this._validateTargetFilter(targetFilter);

    // Store active filter for filtering in event listeners (targetCreated, targetDestroyed, targetInfoChanged)
    this.#discoverTargetFilter = targetFilter;

    if (discover) {
      targetList.on("target-created", this._onTargetCreated);
      targetList.on("target-destroyed", this._onTargetDestroyed);

      for (const target of targetList) {
        this._onTargetCreated("target-created", target);
      }
    } else {
      targetList.off("target-created", this._onTargetCreated);
      targetList.off("target-destroyed", this._onTargetDestroyed);
    }
  }

  async createTarget(options = {}) {
    const { browserContextId, url } = options;

    if (typeof url !== "string") {
      throw new TypeError("url: string value expected");
    }

    let validURL;
    try {
      validURL = Services.io.newURI(url);
    } catch (e) {
      // If we failed to parse given URL, use about:blank instead
      validURL = Services.io.newURI("about:blank");
    }

    const { targetList, window } = this.session.target;
    const onTarget = targetList.once("target-created");
    const tab = await lazy.TabManager.addTab({
      focus: true,
      userContextId: browserContextId,
      window,
    });

    const target = await onTarget;
    if (tab.linkedBrowser != target.browser) {
      throw new Error(
        "Unexpected tab opened: " + tab.linkedBrowser.currentURI.spec
      );
    }

    target.browsingContext.loadURI(validURL, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    return { targetId: target.id };
  }

  async closeTarget(options = {}) {
    const { targetId } = options;
    const { targetList } = this.session.target;
    const target = targetList.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    await lazy.TabManager.removeTab(target.tab);
  }

  async activateTarget(options = {}) {
    const { targetId } = options;
    const { targetList, window } = this.session.target;
    const target = targetList.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    // Focus the window, and select the corresponding tab
    await lazy.windowManager.focusWindow(window);
    await lazy.TabManager.selectTab(target.tab);
  }

  attachToTarget(options = {}) {
    const { targetId } = options;
    const { targetList } = this.session.target;
    const target = targetList.getById(targetId);

    if (!target) {
      throw new Error(`Unable to find target with id '${targetId}'`);
    }

    const tabSession = new lazy.TabSession(
      this.session.connection,
      target,
      lazy.generateUUID()
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
    const attached = [...this.session.connection.sessions.values()].some(
      session => session.target.id === target.id
    );

    return {
      targetId: target.id,
      type: target.type,
      title: target.title,
      url: target.url,
      attached,
      browserContextId: target.browserContextId,
    };
  }

  _filterIncludesTarget(target, filter) {
    for (const entry of filter) {
      if ([undefined, target.type].includes(entry.type)) {
        return !entry.exclude;
      }
    }

    return false;
  }

  _validateTargetFilter(filter) {
    if (!Array.isArray(filter)) {
      throw new TypeError("filter: array value expected");
    }

    for (const entry of filter) {
      if (entry === null || Array.isArray(entry) || typeof entry !== "object") {
        throw new TypeError("filter: object values expected in array");
      }

      if (!["undefined", "string"].includes(typeof entry.type)) {
        throw new TypeError("filter: type: string value expected");
      }

      if (!["undefined", "boolean"].includes(typeof entry.exclude)) {
        throw new TypeError("filter: exclude: boolean value expected");
      }
    }
  }

  _onTargetCreated(eventName, target) {
    if (!this._filterIncludesTarget(target, this.#discoverTargetFilter)) {
      return;
    }

    const targetInfo = this._getTargetInfo(target);
    this.emit("Target.targetCreated", { targetInfo });
  }

  _onTargetDestroyed(eventName, target) {
    if (!this._filterIncludesTarget(target, this.#discoverTargetFilter)) {
      return;
    }

    this.emit("Target.targetDestroyed", {
      targetId: target.id,
    });
  }
}
