/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "uuidGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

const {
  LOAD_FLAGS_BYPASS_CACHE,
  LOAD_FLAGS_BYPASS_PROXY,
  LOAD_FLAGS_NONE,
} = Ci.nsIWebNavigation;

class Page extends ContentProcessDomain {
  constructor(session) {
    super(session);

    this.enabled = false;
    this.lifecycleEnabled = false;
    // script id => { source, worldName }
    this.scriptsToEvaluateOnLoad = new Map();
    this.worldsToEvaluateOnLoad = new Set();

    // This map is used to keep a reference to the loader id for
    // those Page events, which do not directly rely on
    // Network events. This might be a temporary solution until
    // the Network observer could be queried for that. But right
    // now this lives in the parent process.
    this.frameIdToLoaderId = new Map();

    this._onFrameAttached = this._onFrameAttached.bind(this);
    this._onFrameDetached = this._onFrameDetached.bind(this);
    this._onFrameNavigated = this._onFrameNavigated.bind(this);
    this._onScriptLoaded = this._onScriptLoaded.bind(this);

    this.session.contextObserver.on("script-loaded", this._onScriptLoaded);
  }

  destructor() {
    this.setLifecycleEventsEnabled({ enabled: false });
    this.session.contextObserver.off("script-loaded", this._onScriptLoaded);
    this.disable();

    super.destructor();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.session.contextObserver.on(
        "docshell-created",
        this._onFrameAttached
      );
      this.session.contextObserver.on(
        "docshell-destroyed",
        this._onFrameDetached
      );
      this.session.contextObserver.on(
        "frame-navigated",
        this._onFrameNavigated
      );

      this.chromeEventHandler.addEventListener("readystatechange", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.addEventListener("pagehide", this, {
        mozSystemGroup: true,
      });
      this.chromeEventHandler.addEventListener("unload", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.addEventListener("DOMContentLoaded", this, {
        mozSystemGroup: true,
      });
      this.chromeEventHandler.addEventListener("load", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.addEventListener("pageshow", this, {
        mozSystemGroup: true,
      });

      this.enabled = true;
    }
  }

  disable() {
    if (this.enabled) {
      this.session.contextObserver.off(
        "docshell-created",
        this._onFrameAttached
      );
      this.session.contextObserver.off(
        "docshell-destroyed",
        this._onFrameDetached
      );
      this.session.contextObserver.off(
        "frame-navigated",
        this._onFrameNavigated
      );

      this.chromeEventHandler.removeEventListener("readystatechange", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.removeEventListener("pagehide", this, {
        mozSystemGroup: true,
      });
      this.chromeEventHandler.removeEventListener("unload", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.removeEventListener("DOMContentLoaded", this, {
        mozSystemGroup: true,
      });
      this.chromeEventHandler.removeEventListener("load", this, {
        mozSystemGroup: true,
        capture: true,
      });
      this.chromeEventHandler.removeEventListener("pageshow", this, {
        mozSystemGroup: true,
      });
      this.enabled = false;
    }
  }

  async reload(options = {}) {
    const { ignoreCache } = options;
    let flags = LOAD_FLAGS_NONE;
    if (ignoreCache) {
      flags |= LOAD_FLAGS_BYPASS_CACHE;
      flags |= LOAD_FLAGS_BYPASS_PROXY;
    }
    this.docShell.reload(flags);
  }

  getFrameTree() {
    const getFrames = context => {
      const frameTree = {
        frame: this._getFrameDetails({ context }),
      };

      if (context.children.length > 0) {
        const frames = [];
        for (const childContext of context.children) {
          frames.push(getFrames(childContext));
        }
        frameTree.childFrames = frames;
      }

      return frameTree;
    };

    return {
      frameTree: getFrames(this.docShell.browsingContext),
    };
  }

  /**
   * Enqueues given script to be evaluated in every frame upon creation
   *
   * If `worldName` is specified, creates an execution context with the given name
   * and evaluates given script in it.
   *
   * At this time, queued scripts do not get evaluated, hence `source` is marked as
   * "unsupported".
   *
   * @param {Object} options
   * @param {string} options.source (not supported)
   * @param {string=} options.worldName
   * @return {string} Page.ScriptIdentifier
   */
  addScriptToEvaluateOnNewDocument(options = {}) {
    const { source, worldName } = options;
    if (worldName) {
      this.worldsToEvaluateOnLoad.add(worldName);
    }
    const identifier = uuidGen
      .generateUUID()
      .toString()
      .slice(1, -1);
    this.scriptsToEvaluateOnLoad.set(identifier, { worldName, source });

    return { identifier };
  }

  /**
   * Creates an isolated world for the given frame.
   *
   * Really it just creates an execution context with label "isolated".
   *
   * @param {Object} options
   * @param {string} options.frameId
   *     Id of the frame in which the isolated world should be created.
   * @param {string=} options.worldName
   *     An optional name which is reported in the Execution Context.
   * @param {boolean=} options.grantUniversalAccess (not supported)
   *     This is a powerful option, use with caution.
   *
   * @return {number} Runtime.ExecutionContextId
   *     Execution context of the isolated world.
   */
  createIsolatedWorld(options = {}) {
    const { frameId, worldName } = options;

    if (typeof frameId != "string") {
      throw new TypeError("frameId: string value expected");
    }

    if (!["undefined", "string"].includes(typeof worldName)) {
      throw new TypeError("worldName: string value expected");
    }

    const Runtime = this.session.domains.get("Runtime");
    const contexts = Runtime._getContextsForFrame(frameId);
    if (contexts.length == 0) {
      throw new Error("No frame for given id found");
    }

    const defaultContext = Runtime._getDefaultContextForWindow(
      contexts[0].windowId
    );
    const window = defaultContext.window;

    const executionContextId = Runtime._onContextCreated("context-created", {
      windowId: window.windowGlobalChild.innerWindowId,
      window,
      isDefault: false,
      contextName: worldName,
      contextType: "isolated",
    });

    return { executionContextId };
  }

  /**
   * Controls whether page will emit lifecycle events.
   *
   * @param {Object} options
   * @param {boolean} options.enabled
   *     If true, starts emitting lifecycle events.
   */
  setLifecycleEventsEnabled(options = {}) {
    const { enabled } = options;

    this.lifecycleEnabled = enabled;
  }

  url() {
    return this.content.location.href;
  }

  _onFrameAttached(name, { id }) {
    const bc = BrowsingContext.get(id);

    // Don't emit for top-level browsing contexts
    if (!bc.parent) {
      return;
    }

    // TODO: Use a unique identifier for frames (bug 1605359)
    this.emit("Page.frameAttached", {
      frameId: bc.id.toString(),
      parentFrameId: bc.parent.id.toString(),
      stack: null,
    });

    const loaderId = this.frameIdToLoaderId.get(bc.id);
    const timestamp = Date.now() / 1000;
    this.emit("Page.frameStartedLoading", { frameId: bc.id.toString() });
    this.emitLifecycleEvent(bc.id, loaderId, "init", timestamp);
  }

  _onFrameDetached(name, { id }) {
    const bc = BrowsingContext.get(id);

    // Don't emit for top-level browsing contexts
    if (!bc.parent) {
      return;
    }

    // TODO: Use a unique identifier for frames (bug 1605359)
    this.emit("Page.frameDetached", { frameId: bc.id.toString() });
  }

  _onFrameNavigated(name, { frameId }) {
    const bc = BrowsingContext.get(frameId);

    this.emit("Page.frameNavigated", {
      frame: this._getFrameDetails({ context: bc }),
    });
  }

  /**
   * @param {Object=} options
   * @param {number} options.windowId
   *     The inner window id of the window the script has been loaded for.
   * @param {Window} options.window
   *     The window object of the document.
   */
  _onScriptLoaded(name, options = {}) {
    const { windowId, window } = options;

    const Runtime = this.session.domains.get("Runtime");
    for (const world of this.worldsToEvaluateOnLoad) {
      Runtime._onContextCreated("context-created", {
        windowId,
        window,
        isDefault: false,
        contextName: world,
        contextType: "isolated",
      });
    }
    // TODO evaluate each onNewDoc script in the appropriate world
  }

  emitLifecycleEvent(frameId, loaderId, name, timestamp) {
    if (this.lifecycleEnabled) {
      this.emit("Page.lifecycleEvent", {
        frameId: frameId.toString(),
        loaderId,
        name,
        timestamp,
      });
    }
  }

  handleEvent({ type, target }) {
    const timestamp = Date.now() / 1000;
    const frameId = target.defaultView.docShell.browsingContext.id;
    const isFrame = !!target.defaultView.docShell.browsingContext.parent;
    const loaderId = this.frameIdToLoaderId.get(frameId);
    const url = target.location.href;

    switch (type) {
      case "DOMContentLoaded":
        if (!isFrame) {
          this.emit("Page.domContentEventFired", { timestamp });
        }
        this.emitLifecycleEvent(
          frameId,
          loaderId,
          "DOMContentLoaded",
          timestamp
        );
        break;

      case "pagehide":
        // Maybe better to bound to "unload" once we can register for this event
        this.emit("Page.frameStartedLoading", { frameId: frameId.toString() });
        this.emitLifecycleEvent(frameId, loaderId, "init", timestamp);
        break;

      case "load":
        if (!isFrame) {
          this.emit("Page.loadEventFired", { timestamp });
        }
        this.emitLifecycleEvent(frameId, loaderId, "load", timestamp);

        // Todo: Only to be emitted for hashchange events (bug 1636453)
        this.emit("Page.navigatedWithinDocument", {
          frameId: frameId.toString(),
          url,
        });

        // XXX this should most likely be sent differently
        this.emit("Page.frameStoppedLoading", { frameId: frameId.toString() });
        break;

      case "readystatechange":
        if (this.content.document.readyState === "loading") {
          this.emitLifecycleEvent(frameId, loaderId, "init", timestamp);
        }
    }
  }

  _updateLoaderId(data) {
    const { frameId, loaderId } = data;

    this.frameIdToLoaderId.set(frameId, loaderId);
  }

  _contentRect() {
    const docEl = this.content.document.documentElement;

    return {
      x: 0,
      y: 0,
      width: docEl.scrollWidth,
      height: docEl.scrollHeight,
    };
  }

  _devicePixelRatio() {
    return this.content.devicePixelRatio;
  }

  _getFrameDetails({ context, id }) {
    const bc = context || BrowsingContext.get(id);
    const frame = bc.embedderElement;

    return {
      id: bc.id.toString(),
      parentId: bc.parent?.id.toString(),
      loaderId: this.frameIdToLoaderId.get(bc.id),
      url: bc.docShell.domWindow.location.href,
      name: frame?.id || frame?.name,
      securityOrigin: null,
      mimeType: null,
    };
  }

  _getScrollbarSize() {
    const scrollbarHeight = {};
    const scrollbarWidth = {};

    this.content.windowUtils.getScrollbarSize(
      false,
      scrollbarWidth,
      scrollbarHeight
    );

    return {
      width: scrollbarWidth.value,
      height: scrollbarHeight.value,
    };
  }

  _layoutViewport() {
    const scrollbarSize = this._getScrollbarSize();

    return {
      pageX: this.content.pageXOffset,
      pageY: this.content.pageYOffset,
      clientWidth: this.content.innerWidth - scrollbarSize.width,
      clientHeight: this.content.innerHeight - scrollbarSize.height,
    };
  }
}
