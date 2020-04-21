/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Page"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);
const { UnsupportedError } = ChromeUtils.import(
  "chrome://remote/content/Error.jsm"
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

    this._onFrameNavigated = this._onFrameNavigated.bind(this);
    this._onScriptLoaded = this._onScriptLoaded.bind(this);

    this.contextObserver.on("script-loaded", this._onScriptLoaded);
  }

  destructor() {
    this.setLifecycleEventsEnabled({ enabled: false });
    this.contextObserver.off("script-loaded", this._onScriptLoaded);
    this.disable();

    super.destructor();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.contextObserver.on("frame-navigated", this._onFrameNavigated);

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
    }
  }

  disable() {
    if (this.enabled) {
      this.contextObserver.off("frame-navigated", this._onFrameNavigated);

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

  async navigate({ url, referrer, transitionType, frameId } = {}) {
    if (frameId && frameId != this.docShell.browsingContext.id.toString()) {
      throw new UnsupportedError("frameId not supported");
    }

    const opts = {
      loadFlags: transitionToLoadFlag(transitionType),
      referrerURI: referrer,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    this.docShell.loadURI(url, opts);

    return {
      frameId: this.docShell.browsingContext.id.toString(),
    };
  }

  async reload({ ignoreCache }) {
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
        frame: this._getFrameDetails(context),
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
   * @param {string=} options.worldName
   * @param {boolean=} options.grantUniversalAccess (not supported)
   *     This is a powerful option, use with caution.
   * @return {number} Runtime.ExecutionContextId
   */
  createIsolatedWorld(options = {}) {
    const { frameId, worldName } = options;
    if (frameId && frameId != this.docShell.browsingContext.id.toString()) {
      throw new UnsupportedError("frameId not supported");
    }
    const Runtime = this.session.domains.get("Runtime");

    const executionContextId = Runtime._onContextCreated("context-created", {
      windowId: this.content.windowUtils.currentInnerWindowID,
      window: this.content,
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

  _onFrameNavigated(name, { frameId, window }) {
    const url = window.location.href;
    this.emit("Page.frameNavigated", {
      frame: {
        id: frameId,
        // frameNavigated is only emitted for the top level document
        // so that it never has a parent.
        parentId: null,
        url,
      },
    });
  }

  _onScriptLoaded(name) {
    const Runtime = this.session.domains.get("Runtime");
    for (const world of this.worldsToEvaluateOnLoad) {
      Runtime._onContextCreated("context-created", {
        windowId: this.content.windowUtils.currentInnerWindowID,
        window: this.content,
        isDefault: false,
        contextName: world,
        contextType: "isolated",
      });
    }
    // TODO evaluate each onNewDoc script in the appropriate world
  }

  emitLifecycleEvent(frameId, loaderId, name, timestamp) {
    if (this.lifecycleEnabled) {
      this.emit("Page.lifecycleEvent", { frameId, loaderId, name, timestamp });
    }
  }

  handleEvent({ type, target }) {
    const isFrame = target.defaultView != this.content;

    if (isFrame) {
      // Ignore iframes for now
      return;
    }

    const timestamp = Date.now();
    const frameId = target.defaultView.docShell.browsingContext.id.toString();
    const url = target.location.href;

    switch (type) {
      case "DOMContentLoaded":
        this.emit("Page.domContentEventFired", { timestamp });
        if (!isFrame) {
          this.emitLifecycleEvent(
            frameId,
            /* loaderId */ null,
            "DOMContentLoaded",
            timestamp
          );
        }
        break;

      case "pagehide":
        // Maybe better to bound to "unload" once we can register for this event
        this.emit("Page.frameStartedLoading", { frameId });
        if (!isFrame) {
          this.emitLifecycleEvent(
            frameId,
            /* loaderId */ null,
            "init",
            timestamp
          );
        }
        break;

      case "load":
        this.emit("Page.loadEventFired", { timestamp });
        if (!isFrame) {
          this.emitLifecycleEvent(
            frameId,
            /* loaderId */ null,
            "load",
            timestamp
          );
        }

        // XXX this should most likely be sent differently
        this.emit("Page.navigatedWithinDocument", { frameId, url });
        this.emit("Page.frameStoppedLoading", { frameId });
        break;

      case "readystatechange":
        if (this.content.document.readState === "loading") {
          this.emitLifecycleEvent(
            frameId,
            /* loaderId */ null,
            "init",
            timestamp
          );
        }
    }
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

  _getFrameDetails(context) {
    const frame = {
      id: context.id.toString(),
      loaderId: null,
      name: null,
      url: context.docShell.domWindow.location.href,
      securityOrigin: null,
      mimeType: null,
    };

    if (context.parent) {
      frame.parentId = context.parent.id.toString();
    }

    return frame;
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

function transitionToLoadFlag(transitionType) {
  switch (transitionType) {
    case "reload":
      return Ci.nsIWebNavigation.LOAD_FLAGS_IS_REFRESH;
    case "link":
    default:
      return Ci.nsIWebNavigation.LOAD_FLAGS_IS_LINK;
  }
}
