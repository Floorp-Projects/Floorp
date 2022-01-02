/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["GeckoDriver"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",

  Addon: "chrome://remote/content/marionette/addon.js",
  AppInfo: "chrome://remote/content/marionette/appinfo.js",
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  atom: "chrome://remote/content/marionette/atom.js",
  browser: "chrome://remote/content/marionette/browser.js",
  capture: "chrome://remote/content/marionette/capture.js",
  clearActionInputState:
    "chrome://remote/content/marionette/actors/MarionetteCommandsChild.jsm",
  clearElementIdCache:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm",
  Context: "chrome://remote/content/marionette/browser.js",
  cookie: "chrome://remote/content/marionette/cookie.js",
  DebounceCallback: "chrome://remote/content/marionette/sync.js",
  disableEventsActor:
    "chrome://remote/content/marionette/actors/MarionetteEventsParent.jsm",
  element: "chrome://remote/content/marionette/element.js",
  enableEventsActor:
    "chrome://remote/content/marionette/actors/MarionetteEventsParent.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  getMarionetteCommandsActorProxy:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm",
  IdlePromise: "chrome://remote/content/marionette/sync.js",
  l10n: "chrome://remote/content/marionette/l10n.js",
  Log: "chrome://remote/content/shared/Log.jsm",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.js",
  modal: "chrome://remote/content/marionette/modal.js",
  navigate: "chrome://remote/content/marionette/navigate.js",
  permissions: "chrome://remote/content/marionette/permissions.js",
  PollPromise: "chrome://remote/content/marionette/sync.js",
  pprint: "chrome://remote/content/shared/Format.jsm",
  print: "chrome://remote/content/marionette/print.js",
  reftest: "chrome://remote/content/marionette/reftest.js",
  registerCommandsActor:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.jsm",
  TimedPromise: "chrome://remote/content/marionette/sync.js",
  Timeouts: "chrome://remote/content/shared/webdriver/Capabilities.jsm",
  UnhandledPromptBehavior:
    "chrome://remote/content/shared/webdriver/Capabilities.jsm",
  unregisterCommandsActor:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm",
  waitForLoadEvent: "chrome://remote/content/marionette/sync.js",
  waitForObserverTopic: "chrome://remote/content/marionette/sync.js",
  WebDriverSession: "chrome://remote/content/shared/webdriver/Session.jsm",
  WebElement: "chrome://remote/content/marionette/element.js",
  WebElementEventTarget: "chrome://remote/content/marionette/dom.js",
  windowManager: "chrome://remote/content/shared/WindowManager.jsm",
  WindowState: "chrome://remote/content/marionette/browser.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () =>
  Log.get(Log.TYPES.MARIONETTE)
);
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const SUPPORTED_STRATEGIES = new Set([
  element.Strategy.ClassName,
  element.Strategy.Selector,
  element.Strategy.ID,
  element.Strategy.Name,
  element.Strategy.LinkText,
  element.Strategy.PartialLinkText,
  element.Strategy.TagName,
  element.Strategy.XPath,
]);

// Timeout used to abort fullscreen, maximize, and minimize
// commands if no window manager is present.
const TIMEOUT_NO_WINDOW_MANAGER = 5000;

/**
 * The Marionette WebDriver services provides a standard conforming
 * implementation of the W3C WebDriver specification.
 *
 * @see {@link https://w3c.github.io/webdriver/webdriver-spec.html}
 * @namespace driver
 */

/**
 * Implements (parts of) the W3C WebDriver protocol.  GeckoDriver lives
 * in chrome space and mediates calls to the current browsing context's actor.
 *
 * Throughout this prototype, functions with the argument <var>cmd</var>'s
 * documentation refers to the contents of the <code>cmd.parameter</code>
 * object.
 *
 * @class GeckoDriver
 *
 * @param {MarionetteServer} server
 *     The instance of Marionette server.
 */
this.GeckoDriver = function(server) {
  this._server = server;

  // WebDriver Session
  this._currentSession = null;

  this.browsers = {};

  // points to current browser
  this.curBrowser = null;
  // top-most chrome window
  this.mainFrame = null;

  // Use content context by default
  this.context = Context.Content;

  // used for modal dialogs or tab modal alerts
  this.dialog = null;
  this.dialogObserver = null;
};

/**
 * The current context decides if commands are executed in chrome- or
 * content space.
 */
Object.defineProperty(GeckoDriver.prototype, "context", {
  get() {
    return this._context;
  },

  set(context) {
    this._context = Context.fromString(context);
  },
});

/**
 * The current WebDriver Session.
 */
Object.defineProperty(GeckoDriver.prototype, "currentSession", {
  get() {
    if (RemoteAgent.webDriverBiDi) {
      return RemoteAgent.webDriverBiDi.session;
    }

    return this._currentSession;
  },
});

/**
 * Returns the current URL of the ChromeWindow or content browser,
 * depending on context.
 *
 * @return {URL}
 *     Read-only property containing the currently loaded URL.
 */
Object.defineProperty(GeckoDriver.prototype, "currentURL", {
  get() {
    const browsingContext = this.getBrowsingContext({ top: true });
    return new URL(browsingContext.currentWindowGlobal.documentURI.spec);
  },
});

/**
 * Returns the title of the ChromeWindow or content browser,
 * depending on context.
 *
 * @return {string}
 *     Read-only property containing the title of the loaded URL.
 */
Object.defineProperty(GeckoDriver.prototype, "title", {
  get() {
    const browsingContext = this.getBrowsingContext({ top: true });
    return browsingContext.currentWindowGlobal.documentTitle;
  },
});

Object.defineProperty(GeckoDriver.prototype, "windowType", {
  get() {
    return this.curBrowser.window.document.documentElement.getAttribute(
      "windowtype"
    );
  },
});

GeckoDriver.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
  "nsISupportsWeakReference",
]);

/**
 * Callback used to observe the creation of new modal or tab modal dialogs
 * during the session's lifetime.
 */
GeckoDriver.prototype.handleModalDialog = function(action, dialog) {
  if (!this.currentSession) {
    return;
  }

  if (action === modal.ACTION_OPENED) {
    this.dialog = new modal.Dialog(() => this.curBrowser, dialog);
    this.getActor().notifyDialogOpened();
  } else if (action === modal.ACTION_CLOSED) {
    this.dialog = null;
  }
};

/**
 * Get the current visible URL.
 */
GeckoDriver.prototype._getCurrentURL = function() {
  const browsingContext = this.getBrowsingContext({ top: true });
  return new URL(browsingContext.currentURI.spec);
};

/**
 * Get the current "MarionetteCommands" parent actor.
 *
 * @param {Object} options
 * @param {boolean=} options.top
 *     If set to true use the window's top-level browsing context for the actor,
 *     otherwise the one from the currently selected frame. Defaults to false.
 *
 * @returns {MarionetteCommandsParent}
 *     The parent actor.
 */
GeckoDriver.prototype.getActor = function(options = {}) {
  return getMarionetteCommandsActorProxy(() =>
    this.getBrowsingContext(options)
  );
};

/**
 * Get the selected BrowsingContext for the current context.
 *
 * @param {Object} options
 * @param {Context=} options.context
 *     Context (content or chrome) for which to retrieve the browsing context.
 *     Defaults to the current one.
 * @param {boolean=} options.parent
 *     If set to true return the window's parent browsing context,
 *     otherwise the one from the currently selected frame. Defaults to false.
 * @param {boolean=} options.top
 *     If set to true return the window's top-level browsing context,
 *     otherwise the one from the currently selected frame. Defaults to false.
 *
 * @return {BrowsingContext}
 *     The browsing context, or `null` if none is available
 */
GeckoDriver.prototype.getBrowsingContext = function(options = {}) {
  const { context = this.context, parent = false, top = false } = options;

  let browsingContext = null;
  if (context === Context.Chrome) {
    browsingContext = this.currentSession?.chromeBrowsingContext;
  } else {
    browsingContext = this.currentSession?.contentBrowsingContext;
  }

  if (browsingContext && parent) {
    browsingContext = browsingContext.parent;
  }

  if (browsingContext && top) {
    browsingContext = browsingContext.top;
  }

  return browsingContext;
};

/**
 * Get the currently selected window.
 *
 * It will return the outer {@link ChromeWindow} previously selected by
 * window handle through {@link #switchToWindow}, or the first window that
 * was registered.
 *
 * @param {Object} options
 * @param {Context=} options.context
 *     Optional name of the context to use for finding the window.
 *     It will be required if a command always needs a specific context,
 *     whether which context is currently set. Defaults to the current
 *     context.
 *
 * @return {ChromeWindow}
 *     The current top-level browsing context.
 */
GeckoDriver.prototype.getCurrentWindow = function(options = {}) {
  const { context = this.context } = options;

  let win = null;
  switch (context) {
    case Context.Chrome:
      if (this.curBrowser) {
        win = this.curBrowser.window;
      }
      break;

    case Context.Content:
      if (this.curBrowser && this.curBrowser.contentBrowser) {
        win = this.curBrowser.window;
      }
      break;
  }

  return win;
};

GeckoDriver.prototype.isReftestBrowser = function(element) {
  return (
    this._reftest &&
    element &&
    element.tagName === "xul:browser" &&
    element.parentElement &&
    element.parentElement.id === "reftest"
  );
};

/**
 * Create a new browsing context for window and add to known browsers.
 *
 * @param {ChromeWindow} win
 *     Window for which we will create a browsing context.
 *
 * @return {string}
 *     Returns the unique server-assigned ID of the window.
 */
GeckoDriver.prototype.addBrowser = function(win) {
  let context = new browser.Context(win, this);
  let winId = windowManager.getIdForWindow(win);

  this.browsers[winId] = context;
  this.curBrowser = this.browsers[winId];
};

/**
 * Recursively get all labeled text.
 *
 * @param {Element} el
 *     The parent element.
 * @param {Array.<string>} lines
 *      Array that holds the text lines.
 */
GeckoDriver.prototype.getVisibleText = function(el, lines) {
  try {
    if (atom.isElementDisplayed(el, this.getCurrentWindow())) {
      if (el.value) {
        lines.push(el.value);
      }
      for (let child in el.childNodes) {
        this.getVisibleText(el.childNodes[child], lines);
      }
    }
  } catch (e) {
    if (el.nodeName == "#text") {
      lines.push(el.textContent);
    }
  }
};

/**
 * Handles registration of new content browsers.  Depending on
 * their type they are either accepted or ignored.
 *
 * @param {xul:browser} browserElement
 */
GeckoDriver.prototype.registerBrowser = function(browserElement) {
  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (
    !AppInfo.isFirefox ||
    browserElement.namespaceURI != XUL_NS ||
    browserElement.nodeName != "browser" ||
    browserElement.getTabBrowser()
  ) {
    this.curBrowser.register(browserElement);
  }
};

/**
 * Create a new WebDriver session.
 *
 * @param {Object} cmd
 * @param {Object.<string, *>=} cmd.parameters
 *     JSON Object containing any of the recognised capabilities as listed
 *     on the `WebDriverSession` class.
 *
 * @return {Object}
 *     Session ID and capabilities offered by the WebDriver service.
 *
 * @throws {SessionNotCreatedError}
 *     If, for whatever reason, a session could not be created.
 */
GeckoDriver.prototype.newSession = async function(cmd) {
  if (this.currentSession) {
    throw new error.SessionNotCreatedError("Maximum number of active sessions");
  }

  const { parameters: capabilities } = cmd;

  try {
    // If the WebDriver BiDi protocol is active always use the Remote Agent
    // to handle the WebDriver session. If it's not the case then Marionette
    // itself needs to handle it, and has to nullify the "webSocketUrl"
    // capability.
    if (RemoteAgent.webDriverBiDi) {
      RemoteAgent.webDriverBiDi.createSession(capabilities);
    } else {
      this._currentSession = new WebDriverSession(capabilities);
      this._currentSession.capabilities.delete("webSocketUrl");
    }

    const win = await windowManager.waitForInitialApplicationWindow();

    if (MarionettePrefs.clickToStart) {
      Services.prompt.alert(
        win,
        "",
        "Click to start execution of marionette tests"
      );
    }

    this.addBrowser(win);
    this.mainFrame = win;

    registerCommandsActor();
    enableEventsActor();

    // Setup observer for modal dialogs
    this.dialogObserver = new modal.DialogObserver(() => this.curBrowser);
    this.dialogObserver.add(this.handleModalDialog.bind(this));

    for (let win of windowManager.windows) {
      const tabBrowser = browser.getTabBrowser(win);

      if (tabBrowser) {
        for (const tab of tabBrowser.tabs) {
          const contentBrowser = browser.getBrowserForTab(tab);
          this.registerBrowser(contentBrowser);
        }
      }

      this.registerListenersForWindow(win);
    }

    if (this.mainFrame) {
      this.currentSession.chromeBrowsingContext = this.mainFrame.browsingContext;
      this.mainFrame.focus();
    }

    if (this.curBrowser.tab) {
      const browsingContext = this.curBrowser.contentBrowser.browsingContext;
      this.currentSession.contentBrowsingContext = browsingContext;

      let resolveNavigation;

      // Prepare a promise that will resolve upon a navigation.
      const onProgressListenerNavigation = new Promise(
        resolve => (resolveNavigation = resolve)
      );

      // Create a basic webprogress listener which will check if the browsing
      // context is ready for the new session on every state change.
      const navigationListener = {
        onStateChange: (progress, request, flag, status) => {
          const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
          if (isStop) {
            resolveNavigation();
          }
        },

        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };

      // Monitor the webprogress listener before checking isLoadingDocument to
      // avoid race conditions.
      browsingContext.webProgress.addProgressListener(
        navigationListener,
        Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
          Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
      );

      if (browsingContext.webProgress.isLoadingDocument) {
        await onProgressListenerNavigation;
      }

      browsingContext.webProgress.removeProgressListener(
        navigationListener,
        Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
          Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
      );

      this.curBrowser.contentBrowser.focus();
    }

    // Check if there is already an open dialog for the selected browser window.
    this.dialog = modal.findModalDialogs(this.curBrowser);

    Services.obs.addObserver(this, "browser-delayed-startup-finished");
  } catch (e) {
    throw new error.SessionNotCreatedError(e);
  }

  return {
    sessionId: this.currentSession.id,
    capabilities: this.currentSession.capabilities,
  };
};

/**
 * Register event listeners for the specified window.
 *
 * @param {ChromeWindow} win
 *     Chrome window to register event listeners for.
 */
GeckoDriver.prototype.registerListenersForWindow = function(win) {
  const tabBrowser = browser.getTabBrowser(win);

  // Listen for any kind of top-level process switch
  tabBrowser?.addEventListener("XULFrameLoaderCreated", this);
};

/**
 * Unregister event listeners for the specified window.
 *
 * @param {ChromeWindow} win
 *     Chrome window to unregister event listeners for.
 */
GeckoDriver.prototype.unregisterListenersForWindow = function(win) {
  const tabBrowser = browser.getTabBrowser(win);

  tabBrowser?.removeEventListener("XULFrameLoaderCreated", this);
};

GeckoDriver.prototype.handleEvent = function({ target, type }) {
  switch (type) {
    case "XULFrameLoaderCreated":
      if (target === this.curBrowser.contentBrowser) {
        logger.trace(
          "Remoteness change detected. Set new top-level browsing context " +
            `to ${target.browsingContext.id}`
        );

        this.currentSession.contentBrowsingContext = target.browsingContext;
      }
      break;
  }
};

GeckoDriver.prototype.observe = function(subject, topic, data) {
  switch (topic) {
    case "browser-delayed-startup-finished":
      this.registerListenersForWindow(subject);
      break;
  }
};

/**
 * Send the current session's capabilities to the client.
 *
 * Capabilities informs the client of which WebDriver features are
 * supported by Firefox and Marionette.  They are immutable for the
 * length of the session.
 *
 * The return value is an immutable map of string keys
 * ("capabilities") to values, which may be of types boolean,
 * numerical or string.
 */
GeckoDriver.prototype.getSessionCapabilities = function() {
  return { capabilities: this.currentSession.capabilities };
};

/**
 * Sets the context of the subsequent commands.
 *
 * All subsequent requests to commands that in some way involve
 * interaction with a browsing context will target the chosen browsing
 * context.
 *
 * @param {string} value
 *     Name of the context to be switched to.  Must be one of "chrome" or
 *     "content".
 *
 * @throws {InvalidArgumentError}
 *     If <var>value</var> is not a string.
 * @throws {WebDriverError}
 *     If <var>value</var> is not a valid browsing context.
 */
GeckoDriver.prototype.setContext = function(cmd) {
  let value = assert.string(cmd.parameters.value);

  this.context = value;
};

/**
 * Gets the context type that is Marionette's current target for
 * browsing context scoped commands.
 *
 * You may choose a context through the {@link #setContext} command.
 *
 * The default browsing context is {@link Context.Content}.
 *
 * @return {Context}
 *     Current context.
 */
GeckoDriver.prototype.getContext = function() {
  return this.context;
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the return value of the function.
 *
 * It is important to note that if the <var>sandboxName</var> parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If he parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} line
 *     Line in the client's program where this script is evaluated.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {JavaScriptError}
 *     If an {@link Error} was thrown whilst evaluating the script.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 */
GeckoDriver.prototype.executeScript = async function(cmd) {
  let { script, args } = cmd.parameters;
  let opts = {
    script: cmd.parameters.script,
    args: cmd.parameters.args,
    sandboxName: cmd.parameters.sandbox,
    newSandbox: cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
  };

  return { value: await this.execute_(script, args, opts) };
};

/**
 * Executes a JavaScript function in the context of the current browsing
 * context, if in content space, or in chrome space otherwise, and returns
 * the object passed to the callback.
 *
 * The callback is always the last argument to the <var>arguments</var>
 * list passed to the function scope of the script.  It can be retrieved
 * as such:
 *
 * <pre><code>
 *     let callback = arguments[arguments.length - 1];
 *     callback("foo");
 *     // "foo" is returned
 * </code></pre>
 *
 * It is important to note that if the <var>sandboxName</var> parameter
 * is left undefined, the script will be evaluated in a mutable sandbox,
 * causing any change it makes on the global state of the document to have
 * lasting side-effects.
 *
 * @param {string} script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebElement)>} args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {string=} sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If the parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} line
 *     Line in the client's program where this script is evaluated.
 *
 * @return {(string|boolean|number|object|WebElement)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {JavaScriptError}
 *     If an Error was thrown whilst evaluating the script.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 */
GeckoDriver.prototype.executeAsyncScript = async function(cmd) {
  let { script, args } = cmd.parameters;
  let opts = {
    script: cmd.parameters.script,
    args: cmd.parameters.args,
    sandboxName: cmd.parameters.sandbox,
    newSandbox: cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
    async: true,
  };

  return { value: await this.execute_(script, args, opts) };
};

GeckoDriver.prototype.execute_ = async function(
  script,
  args = [],
  {
    sandboxName = null,
    newSandbox = false,
    file = "",
    line = 0,
    async = false,
  } = {}
) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  assert.string(script, pprint`Expected "script" to be a string: ${script}`);
  assert.array(args, pprint`Expected script args to be an array: ${args}`);
  if (sandboxName !== null) {
    assert.string(
      sandboxName,
      pprint`Expected sandbox name to be a string: ${sandboxName}`
    );
  }
  assert.boolean(
    newSandbox,
    pprint`Expected newSandbox to be boolean: ${newSandbox}`
  );
  assert.string(file, pprint`Expected file to be a string: ${file}`);
  assert.number(line, pprint`Expected line to be a number: ${line}`);

  let opts = {
    timeout: this.currentSession.timeouts.script,
    sandboxName,
    newSandbox,
    file,
    line,
    async,
  };

  return this.getActor().executeScript(script, args, opts);
};

/**
 * Navigate to given URL.
 *
 * Navigates the current browsing context to the given URL and waits for
 * the document to load or the session's page timeout duration to elapse
 * before returning.
 *
 * The command will return with a failure if there is an error loading
 * the document or the URL is blocked.  This can occur if it fails to
 * reach host, the URL is malformed, or if there is a certificate issue
 * to name some examples.
 *
 * The document is considered successfully loaded when the
 * DOMContentLoaded event on the frame element associated with the
 * current window triggers and document.readyState is "complete".
 *
 * In chrome context it will change the current window's location to
 * the supplied URL and wait until document.readyState equals "complete"
 * or the page timeout duration has elapsed.
 *
 * @param {string} url
 *     URL to navigate to.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.navigateTo = async function(cmd) {
  assert.content(this.context);
  const browsingContext = assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let validURL;
  try {
    validURL = new URL(cmd.parameters.url);
  } catch (e) {
    throw new error.InvalidArgumentError(`Malformed URL: ${e.message}`);
  }

  // Switch to the top-level browsing context before navigating
  this.currentSession.contentBrowsingContext = browsingContext;

  const loadEventExpected = navigate.isLoadEventExpected(
    this._getCurrentURL(),
    {
      future: validURL,
    }
  );

  await navigate.waitForNavigationCompleted(
    this,
    () => {
      navigate.navigateTo(browsingContext, validURL);
    },
    { loadEventExpected }
  );

  this.curBrowser.contentBrowser.focus();
};

/**
 * Get a string representing the current URL.
 *
 * On Desktop this returns a string representation of the URL of the
 * current top level browsing context.  This is equivalent to
 * document.location.href.
 *
 * When in the context of the chrome, this returns the canonical URL
 * of the current resource.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getCurrentUrl = async function() {
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  return this._getCurrentURL().href;
};

/**
 * Gets the current title of the window.
 *
 * @return {string}
 *     Document title of the top-level browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getTitle = async function() {
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  return this.title;
};

/**
 * Gets the current type of the window.
 *
 * @return {string}
 *     Type of window
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getWindowType = function() {
  assert.open(this.getBrowsingContext({ top: true }));

  return this.windowType;
};

/**
 * Gets the page source of the content document.
 *
 * @return {string}
 *     String serialisation of the DOM of the current browsing context's
 *     active document.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getPageSource = async function() {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  return this.getActor().getPageSource();
};

/**
 * Cause the browser to traverse one step backward in the joint history
 * of the current browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.goBack = async function() {
  assert.content(this.context);
  const browsingContext = assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!browsingContext.embedderElement?.canGoBack) {
    return;
  }

  await navigate.waitForNavigationCompleted(this, () => {
    browsingContext.goBack();
  });
};

/**
 * Cause the browser to traverse one step forward in the joint history
 * of the current browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.goForward = async function() {
  assert.content(this.context);
  const browsingContext = assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!browsingContext.embedderElement?.canGoForward) {
    return;
  }

  await navigate.waitForNavigationCompleted(this, () => {
    browsingContext.goForward();
  });
};

/**
 * Causes the browser to reload the page in current top-level browsing
 * context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.refresh = async function() {
  assert.content(this.context);
  const browsingContext = assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  // Switch to the top-level browsing context before navigating
  this.currentSession.contentBrowsingContext = browsingContext;

  await navigate.waitForNavigationCompleted(this, () => {
    navigate.refresh(browsingContext);
  });
};

/**
 * Get the current window's handle. On desktop this typically corresponds
 * to the currently selected tab.
 *
 * For chrome scope it returns the window identifier for the current chrome
 * window for tests interested in managing the chrome window and tab separately.
 *
 * Return an opaque server-assigned identifier to this window that
 * uniquely identifies it within this Marionette instance.  This can
 * be used to switch to this window at a later point.
 *
 * @return {string}
 *     Unique window handle.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getWindowHandle = function() {
  assert.open(this.getBrowsingContext({ top: true }));

  if (this.context == Context.Chrome) {
    return windowManager.getIdForWindow(this.curBrowser.window);
  }
  return windowManager.getIdForBrowser(this.curBrowser.contentBrowser);
};

/**
 * Get a list of top-level browsing contexts. On desktop this typically
 * corresponds to the set of open tabs for browser windows, or the window
 * itself for non-browser chrome windows.
 *
 * For chrome scope it returns identifiers for each open chrome window for
 * tests interested in managing a set of chrome windows and tabs separately.
 *
 * Each window handle is assigned by the server and is guaranteed unique,
 * however the return array does not have a specified ordering.
 *
 * @return {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getWindowHandles = function() {
  if (this.context == Context.Chrome) {
    return windowManager.chromeWindowHandles.map(String);
  }
  return windowManager.windowHandles.map(String);
};

/**
 * Get the current position and size of the browser window currently in focus.
 *
 * Will return the current browser window size in pixels. Refers to
 * window outerWidth and outerHeight values, which include scroll bars,
 * title bars, etc.
 *
 * @return {Object.<string, number>}
 *     Object with |x| and |y| coordinates, and |width| and |height|
 *     of browser window.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getWindowRect = async function() {
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  return this.curBrowser.rect;
};

/**
 * Set the window position and size of the browser on the operating
 * system window manager.
 *
 * The supplied `width` and `height` values refer to the window `outerWidth`
 * and `outerHeight` values, which include browser chrome and OS-level
 * window borders.
 *
 * @param {number} x
 *     X coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} y
 *     Y coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} width
 *     Width to resize the window to.
 * @param {number} height
 *     Height to resize the window to.
 *
 * @return {Object.<string, number>}
 *     Object with `x` and `y` coordinates and `width` and `height`
 *     dimensions.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not applicable to application.
 */
GeckoDriver.prototype.setWindowRect = async function(cmd) {
  assert.firefox();
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let { x, y, width, height } = cmd.parameters;

  const win = this.getCurrentWindow();
  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Maximized:
    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (width != null && height != null) {
    assert.positiveInteger(height);
    assert.positiveInteger(width);

    if (win.outerWidth != width || win.outerHeight != height) {
      win.resizeTo(width, height);
      await new IdlePromise(win);
    }
  }

  if (x != null && y != null) {
    assert.integer(x);
    assert.integer(y);

    if (win.screenX != x || win.screenY != y) {
      win.moveTo(x, y);
      await new IdlePromise(win);
    }
  }

  return this.curBrowser.rect;
};

/**
 * Switch current top-level browsing context by name or server-assigned
 * ID.  Searches for windows by name, then ID.  Content windows take
 * precedence.
 *
 * @param {string} handle
 *     Handle of the window to switch to.
 * @param {boolean=} focus
 *     A boolean value which determines whether to focus
 *     the window. Defaults to true.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.switchToWindow = async function(cmd) {
  const { focus = true, handle } = cmd.parameters;

  assert.string(
    handle,
    pprint`Expected "handle" to be a string, got ${handle}`
  );
  assert.boolean(focus, pprint`Expected "focus" to be a boolean, got ${focus}`);

  const found = windowManager.findWindowByHandle(handle);

  let selected = false;
  if (found) {
    try {
      await this.setWindowHandle(found, focus);
      selected = true;
    } catch (e) {
      logger.error(e);
    }
  }

  if (!selected) {
    throw new error.NoSuchWindowError(`Unable to locate window: ${handle}`);
  }
};

/**
 * Switch the marionette window to a given window. If the browser in
 * the window is unregistered, register that browser and wait for
 * the registration is complete. If |focus| is true then set the focus
 * on the window.
 *
 * @param {Object} winProperties
 *     Object containing window properties such as returned from
 *     :js:func:`GeckoDriver#getWindowProperties`
 * @param {boolean=} focus
 *     A boolean value which determines whether to focus the window.
 *     Defaults to true.
 */
GeckoDriver.prototype.setWindowHandle = async function(
  winProperties,
  focus = true
) {
  if (!(winProperties.id in this.browsers)) {
    // Initialise Marionette if the current chrome window has not been seen
    // before. Also register the initial tab, if one exists.
    this.addBrowser(winProperties.win);
    this.mainFrame = winProperties.win;

    this.currentSession.chromeBrowsingContext = this.mainFrame.browsingContext;

    if (!winProperties.hasTabBrowser) {
      this.currentSession.contentBrowsingContext = null;
    } else {
      const tabBrowser = browser.getTabBrowser(winProperties.win);

      // For chrome windows such as a reftest window, `getTabBrowser` is not
      // a tabbrowser, it is the content browser which should be used here.
      const contentBrowser = tabBrowser.tabs
        ? tabBrowser.selectedBrowser
        : tabBrowser;

      this.currentSession.contentBrowsingContext =
        contentBrowser.browsingContext;
      this.registerBrowser(contentBrowser);
    }
  } else {
    // Otherwise switch to the known chrome window
    this.curBrowser = this.browsers[winProperties.id];
    this.mainFrame = this.curBrowser.window;

    // Activate the tab if it's a content window.
    let tab = null;
    if (winProperties.hasTabBrowser) {
      tab = await this.curBrowser.switchToTab(
        winProperties.tabIndex,
        winProperties.win,
        focus
      );
    }

    this.currentSession.chromeBrowsingContext = this.mainFrame.browsingContext;
    this.currentSession.contentBrowsingContext =
      tab?.linkedBrowser.browsingContext;
  }

  // Check for existing dialogs for the new window
  this.dialog = modal.findModalDialogs(this.curBrowser);

  // If there is an open window modal dialog the underlying chrome window
  // cannot be focused.
  if (focus && !this.dialog?.isWindowModal) {
    await this.curBrowser.focusWindow();
  }
};

/**
 * Set the current browsing context for future commands to the parent
 * of the current browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToParentFrame = async function() {
  let browsingContext = this.getBrowsingContext();
  if (browsingContext && !browsingContext.parent) {
    return;
  }

  browsingContext = assert.open(browsingContext?.parent);

  this.currentSession.contentBrowsingContext = browsingContext;
};

/**
 * Switch to a given frame within the current window.
 *
 * @param {(string|Object)=} element
 *     A web element reference of the frame or its element id.
 * @param {number=} id
 *     The index of the frame to switch to.
 *     If both element and id are not defined, switch to top-level frame.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToFrame = async function(cmd) {
  const { element: el, id } = cmd.parameters;

  if (typeof id == "number") {
    assert.unsignedShort(id, `Expected id to be unsigned short, got ${id}`);
  }

  const top = id == null && el == null;
  assert.open(this.getBrowsingContext({ top }));
  await this._handleUserPrompts();

  // Bug 1495063: Elements should be passed as WebElement reference
  let byFrame;
  if (typeof el == "string") {
    byFrame = WebElement.fromUUID(el, this.context);
  } else if (el) {
    byFrame = WebElement.fromJSON(el);
  }

  const { browsingContext } = await this.getActor({ top }).switchToFrame(
    byFrame || id
  );

  this.currentSession.contentBrowsingContext = browsingContext;
};

GeckoDriver.prototype.getTimeouts = function() {
  return this.currentSession.timeouts;
};

/**
 * Set timeout for page loading, searching, and scripts.
 *
 * @param {Object.<string, number>}
 *     Dictionary of timeout types and their new value, where all timeout
 *     types are optional.
 *
 * @throws {InvalidArgumentError}
 *     If timeout type key is unknown, or the value provided with it is
 *     not an integer.
 */
GeckoDriver.prototype.setTimeouts = function(cmd) {
  // merge with existing timeouts
  let merged = Object.assign(
    this.currentSession.timeouts.toJSON(),
    cmd.parameters
  );

  this.currentSession.timeouts = Timeouts.fromJSON(merged);
};

/** Single tap. */
GeckoDriver.prototype.singleTap = async function(cmd) {
  assert.open(this.getBrowsingContext());

  let { id, x, y } = cmd.parameters;
  let webEl = WebElement.fromUUID(id, this.context);

  await this.getActor().singleTap(
    webEl,
    x,
    y,
    this.currentSession.capabilities
  );
};

/**
 * Perform a series of grouped actions at the specified points in time.
 *
 * @param {Array.<?>} actions
 *     Array of objects that each represent an action sequence.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not yet available in current context.
 */
GeckoDriver.prototype.performActions = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const actions = cmd.parameters.actions;

  await this.getActor().performActions(
    actions,
    this.currentSession.capabilities
  );
};

/**
 * Release all the keys and pointer buttons that are currently depressed.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.releaseActions = async function() {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  await this.getActor().releaseActions();
};

/**
 * Find an element using the indicated search strategy.
 *
 * @param {string} using
 *     Indicates which search method to use.
 * @param {string} value
 *     Value the client is looking for.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElement = async function(cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!SUPPORTED_STRATEGIES.has(using)) {
    throw new error.InvalidSelectorError(`Strategy not supported: ${using}`);
  }

  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = WebElement.fromUUID(el, this.context);
  }

  let opts = {
    startNode,
    timeout: this.currentSession.timeouts.implicit,
    all: false,
  };

  return this.getActor().findElement(using, value, opts);
};

/**
 * Find elements using the indicated search strategy.
 *
 * @param {string} using
 *     Indicates which search method to use.
 * @param {string} value
 *     Value the client is looking for.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 */
GeckoDriver.prototype.findElements = async function(cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!SUPPORTED_STRATEGIES.has(using)) {
    throw new error.InvalidSelectorError(`Strategy not supported: ${using}`);
  }

  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = WebElement.fromUUID(el, this.context);
  }

  let opts = {
    startNode,
    timeout: this.currentSession.timeouts.implicit,
    all: true,
  };

  return this.getActor().findElements(using, value, opts);
};

/**
 * Return the shadow root of an element in the document.
 *
 * @param {id}
 *     A web element id reference.
 * @return {ShadowRoot}
 *     ShadowRoot of the element.
 *
 * @throws {InvalidArgumentError}
 *     If element <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchShadowRoot}
 *     Element does not have a shadow root attached.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in chrome current context.
 */
GeckoDriver.prototype.getShadowRoot = async function(cmd) {
  // Bug 1743541: Add support for chrome scope.
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(
    cmd.parameters.id,
    pprint`Expected "id" to be a string, got ${cmd.parameters.id}`
  );
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getShadowRoot(webEl);
};

/**
 * Return the active element in the document.
 *
 * @return {WebElement}
 *     Active element of the current browsing context's document
 *     element, if the document element is non-null.
 *
 * @throws {NoSuchElementError}
 *     If the document does not have an active element, i.e. if
 *     its document element has been deleted.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.getActiveElement = async function() {
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  return this.getActor().getActiveElement();
};

/**
 * Send click event to element.
 *
 * @param {string} id
 *     Reference ID to the element that will be clicked.
 *
 * @throws {InvalidArgumentError}
 *     If element <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clickElement = async function(cmd) {
  const browsingContext = assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  const actor = this.getActor();

  const loadEventExpected = navigate.isLoadEventExpected(
    this._getCurrentURL(),
    {
      browsingContext,
      target: await actor.getElementAttribute(webEl, "target"),
    }
  );

  await navigate.waitForNavigationCompleted(
    this,
    () => actor.clickElement(webEl, this.currentSession.capabilities),
    {
      loadEventExpected,
      // The click might trigger a navigation, so don't count on it.
      requireBeforeUnload: false,
    }
  );
};

/**
 * Get a given attribute of an element.
 *
 * @param {string} id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} name
 *     Name of the attribute which value to retrieve.
 *
 * @return {string}
 *     Value of the attribute.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementAttribute = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const id = assert.string(cmd.parameters.id);
  const name = assert.string(cmd.parameters.name);
  const webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementAttribute(webEl, name);
};

/**
 * Returns the value of a property associated with given element.
 *
 * @param {string} id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} name
 *     Name of the property which value to retrieve.
 *
 * @return {string}
 *     Value of the property.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementProperty = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const id = assert.string(cmd.parameters.id);
  const name = assert.string(cmd.parameters.name);
  const webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementProperty(webEl, name);
};

/**
 * Get the text of an element, if any.  Includes the text of all child
 * elements.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {string}
 *     Element's text "as rendered".
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementText = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementText(webEl);
};

/**
 * Get the tag name of the element.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {string}
 *     Local tag name of element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementTagName = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementTagName(webEl);
};

/**
 * Check if element is displayed.
 *
 * @param {string} id
 *     Reference ID to the element that will be inspected.
 *
 * @return {boolean}
 *     True if displayed, false otherwise.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementDisplayed = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().isElementDisplayed(
    webEl,
    this.currentSession.capabilities
  );
};

/**
 * Return the property of the computed style of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 * @param {string} propertyName
 *     CSS rule that is being requested.
 *
 * @return {string}
 *     Value of |propertyName|.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>propertyName</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementValueOfCssProperty = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let prop = assert.string(cmd.parameters.propertyName);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementValueOfCssProperty(webEl, prop);
};

/**
 * Check if element is enabled.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 *
 * @return {boolean}
 *     True if enabled, false if disabled.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementEnabled = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().isElementEnabled(
    webEl,
    this.currentSession.capabilities
  );
};

/**
 * Check if element is selected.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 *
 * @return {boolean}
 *     True if selected, false if unselected.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementSelected = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().isElementSelected(
    webEl,
    this.currentSession.capabilities
  );
};

/**
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementRect = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().getElementRect(webEl);
};

/**
 * Send key presses to element after focusing on it.
 *
 * @param {string} id
 *     Reference ID to the element that will be checked.
 * @param {string} text
 *     Value to send to the element.
 *
 * @throws {InvalidArgumentError}
 *     If `id` or `text` are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference `id` is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.sendKeysToElement = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let text = assert.string(cmd.parameters.text);
  let webEl = WebElement.fromUUID(id, this.context);

  return this.getActor().sendKeysToElement(
    webEl,
    text,
    this.currentSession.capabilities
  );
};

/**
 * Clear the text of an element.
 *
 * @param {string} id
 *     Reference ID to the element that will be cleared.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clearElement = async function(cmd) {
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = assert.string(cmd.parameters.id);
  let webEl = WebElement.fromUUID(id, this.context);

  await this.getActor().clearElement(webEl);
};

/**
 * Add a single cookie to the cookie store associated with the active
 * document's address.
 *
 * @param {Map.<string, (string|number|boolean)> cookie
 *     Cookie object.
 *
 * @throws {InvalidCookieDomainError}
 *     If <var>cookie</var> is for a different domain than the active
 *     document's host.
 * @throws {NoSuchWindowError}
 *     Bbrowsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.addCookie = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { protocol, hostname } = this._getCurrentURL();

  const networkSchemes = ["http:", "https:"];
  if (!networkSchemes.includes(protocol)) {
    throw new error.InvalidCookieDomainError("Document is cookie-averse");
  }

  let newCookie = cookie.fromJSON(cmd.parameters.cookie);

  cookie.add(newCookie, { restrictToHost: hostname, protocol });
};

/**
 * Get all the cookies for the current domain.
 *
 * This is the equivalent of calling <code>document.cookie</code> and
 * parsing the result.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.getCookies = async function() {
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  return [...cookie.iter(hostname, pathname)];
};

/**
 * Delete all cookies that are visible to a document.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.deleteAllCookies = async function() {
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  for (let toDelete of cookie.iter(hostname, pathname)) {
    cookie.remove(toDelete);
  }
};

/**
 * Delete a cookie by name.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.deleteCookie = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  let name = assert.string(cmd.parameters.name);
  for (let c of cookie.iter(hostname, pathname)) {
    if (c.name === name) {
      cookie.remove(c);
    }
  }
};

/**
 * Open a new top-level browsing context.
 *
 * @param {string=} type
 *     Optional type of the new top-level browsing context. Can be one of
 *     `tab` or `window`. Defaults to `tab`.
 * @param {boolean=} focus
 *     Optional flag if the new top-level browsing context should be opened
 *     in foreground (focused) or background (not focused). Defaults to false.
 * @param {boolean=} private
 *     Optional flag, which gets only evaluated for type `window`. True if the
 *     new top-level browsing context should be a private window.
 *     Defaults to false.
 *
 * @return {Object.<string, string>}
 *     Handle and type of the new browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.newWindow = async function(cmd) {
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let focus = false;
  if (typeof cmd.parameters.focus != "undefined") {
    focus = assert.boolean(
      cmd.parameters.focus,
      pprint`Expected "focus" to be a boolean, got ${cmd.parameters.focus}`
    );
  }

  let isPrivate = false;
  if (typeof cmd.parameters.private != "undefined") {
    isPrivate = assert.boolean(
      cmd.parameters.private,
      pprint`Expected "private" to be a boolean, got ${cmd.parameters.private}`
    );
  }

  let type;
  if (typeof cmd.parameters.type != "undefined") {
    type = assert.string(
      cmd.parameters.type,
      pprint`Expected "type" to be a string, got ${cmd.parameters.type}`
    );
  }

  // If an invalid or no type has been specified default to a tab.
  if (typeof type == "undefined" || !["tab", "window"].includes(type)) {
    type = "tab";
  }

  let contentBrowser;

  // Actors need the new window to be loaded to safely execute queries.
  // Wait until a load event is dispatched for the new browsing context.
  const onBrowserContentLoaded = waitForLoadEvent(
    "pageshow",
    () => contentBrowser?.browsingContext
  );

  switch (type) {
    case "window":
      let win = await this.curBrowser.openBrowserWindow(focus, isPrivate);
      contentBrowser = browser.getTabBrowser(win).selectedBrowser;
      break;

    default:
      // To not fail if a new type gets added in the future, make opening
      // a new tab the default action.
      let tab = await this.curBrowser.openTab(focus);
      contentBrowser = browser.getBrowserForTab(tab);
  }

  await onBrowserContentLoaded;

  // Wait until the browser is available.
  // TODO: Fix by using `Browser:Init` or equivalent on bug 1311041
  let windowId = await new PollPromise((resolve, reject) => {
    let id = windowManager.getIdForBrowser(contentBrowser);
    windowManager.windowHandles.includes(id) ? resolve(id) : reject();
  });

  return { handle: windowId.toString(), type };
};

/**
 * Close the currently selected tab/window.
 *
 * With multiple open tabs present the currently selected tab will
 * be closed.  Otherwise the window itself will be closed. If it is the
 * last window currently open, the window will not be closed to prevent
 * a shutdown of the application. Instead the returned list of window
 * handles is empty.
 *
 * @return {Array.<string>}
 *     Unique window handles of remaining windows.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.close = async function() {
  assert.open(this.getBrowsingContext({ context: Context.Content, top: true }));
  await this._handleUserPrompts();

  let nwins = 0;

  for (let win of windowManager.windows) {
    // For browser windows count the tabs. Otherwise take the window itself.
    let tabbrowser = browser.getTabBrowser(win);
    if (tabbrowser && tabbrowser.tabs) {
      nwins += tabbrowser.tabs.length;
    } else {
      nwins += 1;
    }
  }

  // If there is only one window left, do not close it. Instead return
  // a faked empty array of window handles.  This will instruct geckodriver
  // to terminate the application.
  if (nwins === 1) {
    return [];
  }

  await this.curBrowser.closeTab();
  this.currentSession.contentBrowsingContext = null;

  return windowManager.windowHandles.map(String);
};

/**
 * Close the currently selected chrome window.
 *
 * If it is the last window currently open, the chrome window will not be
 * closed to prevent a shutdown of the application. Instead the returned
 * list of chrome window handles is empty.
 *
 * @return {Array.<string>}
 *     Unique chrome window handles of remaining chrome windows.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.closeChromeWindow = async function() {
  assert.firefox();
  assert.open(this.getBrowsingContext({ context: Context.Chrome, top: true }));

  let nwins = 0;

  // eslint-disable-next-line
  for (let _ of windowManager.windows) {
    nwins++;
  }

  // If there is only one window left, do not close it.  Instead return
  // a faked empty array of window handles. This will instruct geckodriver
  // to terminate the application.
  if (nwins == 1) {
    return [];
  }

  await this.curBrowser.closeWindow();
  this.currentSession.chromeBrowsingContext = null;
  this.currentSession.contentBrowsingContext = null;

  return windowManager.chromeWindowHandles.map(String);
};

/** Delete Marionette session. */
GeckoDriver.prototype.deleteSession = function() {
  if (!this.currentSession) {
    return;
  }

  for (let win of windowManager.windows) {
    this.unregisterListenersForWindow(win);
  }

  // reset to the top-most frame
  this.mainFrame = null;

  if (this.dialogObserver) {
    this.dialogObserver.cleanup();
    this.dialogObserver = null;
  }

  Services.obs.removeObserver(this, "browser-delayed-startup-finished");

  clearActionInputState();
  clearElementIdCache();

  // Always unregister actors after all other observers
  // and listeners have been removed.
  unregisterCommandsActor();
  // MarionetteEvents actors are only disabled to avoid IPC errors if there are
  // in flight events being forwarded from the content process to the parent
  // process.
  disableEventsActor();

  if (RemoteAgent.webDriverBiDi) {
    RemoteAgent.webDriverBiDi.deleteSession();
  } else {
    this.currentSession.destroy();
    this._currentSession = null;
  }
};

/**
 * Takes a screenshot of a web element, current frame, or viewport.
 *
 * The screen capture is returned as a lossless PNG image encoded as
 * a base 64 string.
 *
 * If called in the content context, the |id| argument is not null and
 * refers to a present and visible web element's ID, the capture area will
 * be limited to the bounding box of that element.  Otherwise, the capture
 * area will be the bounding box of the current frame.
 *
 * If called in the chrome context, the screenshot will always represent
 * the entire viewport.
 *
 * @param {string=} id
 *     Optional web element reference to take a screenshot of.
 *     If undefined, a screenshot will be taken of the document element.
 * @param {boolean=} full
 *     True to take a screenshot of the entire document element. Is only
 *     considered if <var>id</var> is not defined. Defaults to true.
 * @param {boolean=} hash
 *     True if the user requests a hash of the image data. Defaults to false.
 * @param {boolean=} scroll
 *     Scroll to element if |id| is provided. Defaults to true.
 *
 * @return {string}
 *     If <var>hash</var> is false, PNG image encoded as Base64 encoded
 *     string.  If <var>hash</var> is true, hex digest of the SHA-256
 *     hash of the Base64 encoded string.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.takeScreenshot = async function(cmd) {
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let { id, full, hash, scroll } = cmd.parameters;
  let format = hash ? capture.Format.Hash : capture.Format.Base64;

  full = typeof full == "undefined" ? true : full;
  scroll = typeof scroll == "undefined" ? true : scroll;

  let webEl = id ? WebElement.fromUUID(id, this.context) : null;

  // Only consider full screenshot if no element has been specified
  full = webEl ? false : full;

  return this.getActor().takeScreenshot(webEl, format, full, scroll);
};

/**
 * Get the current browser orientation.
 *
 * Will return one of the valid primary orientation values
 * portrait-primary, landscape-primary, portrait-secondary, or
 * landscape-secondary.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getScreenOrientation = function() {
  assert.mobile();
  assert.open(this.getBrowsingContext({ top: true }));

  const win = this.getCurrentWindow();

  return win.screen.mozOrientation;
};

/**
 * Set the current browser orientation.
 *
 * The supplied orientation should be given as one of the valid
 * orientation values.  If the orientation is unknown, an error will
 * be raised.
 *
 * Valid orientations are "portrait" and "landscape", which fall
 * back to "portrait-primary" and "landscape-primary" respectively,
 * and "portrait-secondary" as well as "landscape-secondary".
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.setScreenOrientation = function(cmd) {
  assert.mobile();
  assert.open(this.getBrowsingContext({ top: true }));

  const ors = [
    "portrait",
    "landscape",
    "portrait-primary",
    "landscape-primary",
    "portrait-secondary",
    "landscape-secondary",
  ];

  let or = String(cmd.parameters.orientation);
  assert.string(or);
  let mozOr = or.toLowerCase();
  if (!ors.includes(mozOr)) {
    throw new error.InvalidArgumentError(`Unknown screen orientation: ${or}`);
  }

  const win = this.getCurrentWindow();
  if (!win.screen.mozLockOrientation(mozOr)) {
    throw new error.WebDriverError(`Unable to set screen orientation: ${or}`);
  }
};

/**
 * Synchronously minimizes the user agent window as if the user pressed
 * the minimize button.
 *
 * No action is taken if the window is already minimized.
 *
 * Not supported on Fennec.
 *
 * @return {Object.<string, number>}
 *     Window rect and window state.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.minimizeWindow = async function() {
  assert.firefox();
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Maximized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Minimized) {
    let cb;
    let observer = new WebElementEventTarget(this.curBrowser.messageManager);
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        observer.addEventListener("visibilitychange", cb);
        win.minimize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    observer.removeEventListener("visibilitychange", cb);
    await new IdlePromise(win);
  }

  return this.curBrowser.rect;
};

/**
 * Synchronously maximizes the user agent window as if the user pressed
 * the maximize button.
 *
 * No action is taken if the window is already maximized.
 *
 * Not supported on Fennec.
 *
 * @return {Object.<string, number>}
 *     Window rect.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.maximizeWindow = async function() {
  assert.firefox();
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (WindowState.from(win.windowState)) {
    case WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Maximized) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.maximize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
    await new IdlePromise(win);
  }

  return this.curBrowser.rect;
};

/**
 * Synchronously sets the user agent window to full screen as if the user
 * had done "View > Enter Full Screen".
 *
 * No action is taken if the window is already in full screen mode.
 *
 * Not supported on Fennec.
 *
 * @return {Map.<string, number>}
 *     Window rect.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.fullscreenWindow = async function() {
  assert.firefox();
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (WindowState.from(win.windowState)) {
    case WindowState.Maximized:
    case WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (WindowState.from(win.windowState) != WindowState.Fullscreen) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new TimedPromise(
      resolve => {
        cb = new DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.fullScreen = true;
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
  }
  await new IdlePromise(win);

  return this.curBrowser.rect;
};

/**
 * Dismisses a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.dismissDialog = async function() {
  assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  const dialogClosed = this.dialogObserver.dialogClosed();

  const { button0, button1 } = this.dialog.ui;
  (button1 ? button1 : button0).click();

  await dialogClosed;

  const win = this.getCurrentWindow();
  await new IdlePromise(win);
};

/**
 * Accepts a currently displayed tab modal, or returns no such alert if
 * no modal is displayed.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.acceptDialog = async function() {
  assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  const dialogClosed = this.dialogObserver.dialogClosed();

  const { button0 } = this.dialog.ui;
  button0.click();

  await dialogClosed;

  const win = this.getCurrentWindow();
  await new IdlePromise(win);
};

/**
 * Returns the message shown in a currently displayed modal, or returns
 * a no such alert error if no modal is currently displayed.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getTextFromDialog = function() {
  assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  return this.dialog.ui.infoBody.textContent;
};

/**
 * Set the user prompt's value field.
 *
 * Sends keys to the input field of a currently displayed modal, or
 * returns a no such alert error if no modal is currently displayed. If
 * a tab modal is currently displayed but has no means for text input,
 * an element not visible error is returned.
 *
 * @param {string} text
 *     Input to the user prompt's value field.
 *
 * @throws {ElementNotInteractableError}
 *     If the current user prompt is an alert or confirm.
 * @throws {NoSuchAlertError}
 *     If there is no current user prompt.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnsupportedOperationError}
 *     If the current user prompt is something other than an alert,
 *     confirm, or a prompt.
 */
GeckoDriver.prototype.sendKeysToDialog = async function(cmd) {
  assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  let text = assert.string(cmd.parameters.text);
  let promptType = this.dialog.args.promptType;

  switch (promptType) {
    case "alert":
    case "confirm":
      throw new error.ElementNotInteractableError(
        `User prompt of type ${promptType} is not interactable`
      );
    case "prompt":
      break;
    default:
      await this.dismissDialog();
      throw new error.UnsupportedOperationError(
        `User prompt of type ${promptType} is not supported`
      );
  }

  // see toolkit/components/prompts/content/commonDialog.js
  let { loginTextbox } = this.dialog.ui;
  loginTextbox.value = text;
};

GeckoDriver.prototype._checkIfAlertIsPresent = function() {
  if (!this.dialog || !this.dialog.ui) {
    throw new error.NoSuchAlertError();
  }
};

GeckoDriver.prototype._handleUserPrompts = async function() {
  if (!this.dialog || !this.dialog.ui) {
    return;
  }

  let { textContent } = this.dialog.ui.infoBody;

  const behavior = this.currentSession.unhandledPromptBehavior;
  switch (behavior) {
    case UnhandledPromptBehavior.Accept:
      await this.acceptDialog();
      break;

    case UnhandledPromptBehavior.AcceptAndNotify:
      await this.acceptDialog();
      throw new error.UnexpectedAlertOpenError(
        `Accepted user prompt dialog: ${textContent}`
      );

    case UnhandledPromptBehavior.Dismiss:
      await this.dismissDialog();
      break;

    case UnhandledPromptBehavior.DismissAndNotify:
      await this.dismissDialog();
      throw new error.UnexpectedAlertOpenError(
        `Dismissed user prompt dialog: ${textContent}`
      );

    case UnhandledPromptBehavior.Ignore:
      throw new error.UnexpectedAlertOpenError(
        "Encountered unhandled user prompt dialog"
      );

    default:
      throw new TypeError(`Unknown unhandledPromptBehavior "${behavior}"`);
  }
};

/**
 * Enables or disables accepting new socket connections.
 *
 * By calling this method with `false` the server will not accept any
 * further connections, but existing connections will not be forcible
 * closed. Use `true` to re-enable accepting connections.
 *
 * Please note that when closing the connection via the client you can
 * end-up in a non-recoverable state if it hasn't been enabled before.
 *
 * This method is used for custom in application shutdowns via
 * marionette.quit() or marionette.restart(), like File -> Quit.
 *
 * @param {boolean} state
 *     True if the server should accept new socket connections.
 */
GeckoDriver.prototype.acceptConnections = function(cmd) {
  assert.boolean(cmd.parameters.value);
  this._server.acceptConnections = cmd.parameters.value;
};

/**
 * Quits the application with the provided flags.
 *
 * Marionette will stop accepting new connections before ending the
 * current session, and finally attempting to quit the application.
 *
 * Optional {@link nsIAppStartup} flags may be provided as
 * an array of masks, and these will be combined by ORing
 * them with a bitmask.  The available masks are defined in
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Reference/Interface/nsIAppStartup.
 *
 * Crucially, only one of the *Quit flags can be specified. The |eRestart|
 * flag may be bit-wise combined with one of the *Quit flags to cause
 * the application to restart after it quits.
 *
 * @param {Array.<string>=} flags
 *     Constant name of masks to pass to |Services.startup.quit|.
 *     If empty or undefined, |nsIAppStartup.eAttemptQuit| is used.
 *
 * @return {Object<string,boolean>}
 *     Dictionary containing information that explains the shutdown reason.
 *     The value for `cause` contains the shutdown kind like "shutdown" or
 *     "restart", while `forced` will indicate if it was a normal or forced
 *     shutdown of the application.
 *
 * @throws {InvalidArgumentError}
 *     If <var>flags</var> contains unknown or incompatible flags,
 *     for example multiple Quit flags.
 */
GeckoDriver.prototype.quit = async function(cmd) {
  const quits = ["eConsiderQuit", "eAttemptQuit", "eForceQuit"];

  let flags = [];
  if (typeof cmd.parameters.flags != "undefined") {
    flags = assert.array(cmd.parameters.flags);
  }

  let quitSeen;
  let mode = 0;
  if (flags.length > 0) {
    for (let k of flags) {
      assert.in(k, Ci.nsIAppStartup);

      if (quits.includes(k)) {
        if (quitSeen) {
          throw new error.InvalidArgumentError(
            `${k} cannot be combined with ${quitSeen}`
          );
        }
        quitSeen = k;
      }

      mode |= Ci.nsIAppStartup[k];
    }
  } else {
    mode = Ci.nsIAppStartup.eAttemptQuit;
  }

  this._server.acceptConnections = false;
  this.deleteSession();

  // Notify all windows that an application quit has been requested.
  const cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
    Ci.nsISupportsPRBool
  );
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

  // If the shutdown of the application is prevented force quit it instead.
  if (cancelQuit.data) {
    mode |= Ci.nsIAppStartup.eForceQuit;
  }

  // delay response until the application is about to quit
  let quitApplication = waitForObserverTopic("quit-application");
  Services.startup.quit(mode);

  return {
    cause: (await quitApplication).data,
    forced: cancelQuit.data,
  };
};

GeckoDriver.prototype.installAddon = function(cmd) {
  assert.desktop();

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (
    typeof path == "undefined" ||
    typeof path != "string" ||
    typeof temp != "boolean"
  ) {
    throw new error.InvalidArgumentError();
  }

  return Addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function(cmd) {
  assert.firefox();

  let id = cmd.parameters.id;
  if (typeof id == "undefined" || typeof id != "string") {
    throw new error.InvalidArgumentError();
  }

  return Addon.uninstall(id);
};

/**
 * Retrieve the localized string for the specified entity id.
 *
 * Example:
 *     localizeEntity(["chrome://branding/locale/brand.dtd"], "brandShortName")
 *
 * @param {Array.<string>} urls
 *     Array of .dtd URLs.
 * @param {string} id
 *     The ID of the entity to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested entity.
 */
GeckoDriver.prototype.localizeEntity = function(cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new error.InvalidArgumentError(
      "Value of `urls` should be of type 'Array'"
    );
  }
  if (typeof id != "string") {
    throw new error.InvalidArgumentError(
      "Value of `id` should be of type 'string'"
    );
  }

  return l10n.localizeEntity(urls, id);
};

/**
 * Retrieve the localized string for the specified property id.
 *
 * Example:
 *
 *     localizeProperty(
 *         ["chrome://global/locale/findbar.properties"], "FastFind");
 *
 * @param {Array.<string>} urls
 *     Array of .properties URLs.
 * @param {string} id
 *     The ID of the property to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested property.
 */
GeckoDriver.prototype.localizeProperty = function(cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new error.InvalidArgumentError(
      "Value of `urls` should be of type 'Array'"
    );
  }
  if (typeof id != "string") {
    throw new error.InvalidArgumentError(
      "Value of `id` should be of type 'string'"
    );
  }

  return l10n.localizeProperty(urls, id);
};

/**
 * Initialize the reftest mode
 */
GeckoDriver.prototype.setupReftest = async function(cmd) {
  if (this._reftest) {
    throw new error.UnsupportedOperationError(
      "Called reftest:setup with a reftest session already active"
    );
  }

  let {
    urlCount = {},
    screenshot = "unexpected",
    isPrint = false,
  } = cmd.parameters;
  if (!["always", "fail", "unexpected"].includes(screenshot)) {
    throw new error.InvalidArgumentError(
      "Value of `screenshot` should be 'always', 'fail' or 'unexpected'"
    );
  }

  this._reftest = new reftest.Runner(this);
  this._reftest.setup(urlCount, screenshot, isPrint);
};

/** Run a reftest. */
GeckoDriver.prototype.runReftest = async function(cmd) {
  let {
    test,
    references,
    expected,
    timeout,
    width,
    height,
    pageRanges,
  } = cmd.parameters;

  if (!this._reftest) {
    throw new error.UnsupportedOperationError(
      "Called reftest:run before reftest:start"
    );
  }

  assert.string(test);
  assert.string(expected);
  assert.array(references);

  return {
    value: await this._reftest.run(
      test,
      references,
      expected,
      timeout,
      pageRanges,
      width,
      height
    ),
  };
};

/**
 * End a reftest run.
 *
 * Closes the reftest window (without changing the current window handle),
 * and removes cached canvases.
 */
GeckoDriver.prototype.teardownReftest = function() {
  if (!this._reftest) {
    throw new error.UnsupportedOperationError(
      "Called reftest:teardown before reftest:start"
    );
  }

  this._reftest.teardown();
  this._reftest = null;
};

/**
 * Print page as PDF.
 *
 * @param {boolean=} landscape
 *     Paper orientation. Defaults to false.
 * @param {number=} margin.bottom
 *     Bottom margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.left
 *     Left margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.right
 *     Right margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} margin.top
 *     Top margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {Array.<string|number>=} pageRanges
 *     Paper ranges to print, e.g., ['1-5', 8, '11-13'].
 *     Defaults to the empty array, which means print all pages.
 * @param {number=} page.height
 *     Paper height in cm. Defaults to US letter height (11 inches / 27.94cm)
 * @param {number=} page.width
 *     Paper width in cm. Defaults to US letter width (8.5 inches / 21.59cm)
 * @param {boolean=} shrinkToFit
 *     Whether or not to override page size as defined by CSS.
 *     Defaults to true, in which case the content will be scaled
 *     to fit the paper size.
 * @param {boolean=} printBackground
 *     Print background graphics. Defaults to false.
 * @param {number=} scale
 *     Scale of the webpage rendering. Defaults to 1.
 *
 * @return {string}
 *     Base64 encoded PDF representing printed document
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.print = async function(cmd) {
  assert.content(this.context);
  assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const settings = print.addDefaultSettings(cmd.parameters);
  for (let prop of ["top", "bottom", "left", "right"]) {
    assert.positiveNumber(
      settings.margin[prop],
      pprint`margin.${prop} is not a positive number`
    );
  }
  for (let prop of ["width", "height"]) {
    assert.positiveNumber(
      settings.page[prop],
      pprint`page.${prop} is not a positive number`
    );
  }
  assert.positiveNumber(
    settings.scale,
    `scale ${settings.scale} is not a positive number`
  );
  assert.that(
    s => s >= print.minScaleValue && settings.scale <= print.maxScaleValue,
    `scale ${settings.scale} is outside the range ${print.minScaleValue}-${print.maxScaleValue}`
  )(settings.scale);
  assert.boolean(settings.shrinkToFit);
  assert.boolean(settings.landscape);
  assert.boolean(settings.printBackground);
  assert.array(settings.pageRanges);

  const linkedBrowser = this.curBrowser.tab.linkedBrowser;
  const filePath = await print.printToFile(linkedBrowser, settings);

  // return all data as a base64 encoded string
  let bytes;
  try {
    bytes = await IOUtils.read(filePath);
  } finally {
    await IOUtils.remove(filePath);
  }

  // Each UCS2 character has an upper byte of 0 and a lower byte matching
  // the binary data. Splitting the file into chunks to avoid hitting the
  // internal argument length limit.
  const chunks = [];
  // This is the largest power of 2 smaller than MAX_ARGS_LENGTH defined in Spidermonkey
  const argLengthLimit = 262144;

  for (let offset = 0; offset < bytes.length; offset += argLengthLimit) {
    const chunkData = bytes.subarray(offset, offset + argLengthLimit);

    chunks.push(String.fromCharCode.apply(null, chunkData));
  }

  return {
    value: btoa(chunks.join("")),
  };
};

GeckoDriver.prototype.setPermission = async function(cmd) {
  const { descriptor, state, oneRealm = false } = cmd.parameters;

  assert.boolean(oneRealm);
  assert.that(
    state => ["granted", "denied", "prompt"].includes(state),
    `state is ${state}, expected "granted", "denied", or "prompt"`
  )(state);

  permissions.set(descriptor, state, oneRealm);
};

GeckoDriver.prototype.commands = {
  // Marionette service
  "Marionette:AcceptConnections": GeckoDriver.prototype.acceptConnections,
  "Marionette:GetContext": GeckoDriver.prototype.getContext,
  "Marionette:GetScreenOrientation": GeckoDriver.prototype.getScreenOrientation,
  "Marionette:GetWindowType": GeckoDriver.prototype.getWindowType,
  "Marionette:Quit": GeckoDriver.prototype.quit,
  "Marionette:SetContext": GeckoDriver.prototype.setContext,
  "Marionette:SetScreenOrientation": GeckoDriver.prototype.setScreenOrientation,
  "Marionette:SingleTap": GeckoDriver.prototype.singleTap,

  // Addon service
  "Addon:Install": GeckoDriver.prototype.installAddon,
  "Addon:Uninstall": GeckoDriver.prototype.uninstallAddon,

  // L10n service
  "L10n:LocalizeEntity": GeckoDriver.prototype.localizeEntity,
  "L10n:LocalizeProperty": GeckoDriver.prototype.localizeProperty,

  // Reftest service
  "reftest:setup": GeckoDriver.prototype.setupReftest,
  "reftest:run": GeckoDriver.prototype.runReftest,
  "reftest:teardown": GeckoDriver.prototype.teardownReftest,

  // WebDriver service
  "WebDriver:AcceptAlert": GeckoDriver.prototype.acceptDialog,
  // deprecated, no longer used since the geckodriver 0.30.0 release
  "WebDriver:AcceptDialog": GeckoDriver.prototype.acceptDialog,
  "WebDriver:AddCookie": GeckoDriver.prototype.addCookie,
  "WebDriver:Back": GeckoDriver.prototype.goBack,
  "WebDriver:CloseChromeWindow": GeckoDriver.prototype.closeChromeWindow,
  "WebDriver:CloseWindow": GeckoDriver.prototype.close,
  "WebDriver:DeleteAllCookies": GeckoDriver.prototype.deleteAllCookies,
  "WebDriver:DeleteCookie": GeckoDriver.prototype.deleteCookie,
  "WebDriver:DeleteSession": GeckoDriver.prototype.deleteSession,
  "WebDriver:DismissAlert": GeckoDriver.prototype.dismissDialog,
  "WebDriver:ElementClear": GeckoDriver.prototype.clearElement,
  "WebDriver:ElementClick": GeckoDriver.prototype.clickElement,
  "WebDriver:ElementSendKeys": GeckoDriver.prototype.sendKeysToElement,
  "WebDriver:ExecuteAsyncScript": GeckoDriver.prototype.executeAsyncScript,
  "WebDriver:ExecuteScript": GeckoDriver.prototype.executeScript,
  "WebDriver:FindElement": GeckoDriver.prototype.findElement,
  "WebDriver:FindElements": GeckoDriver.prototype.findElements,
  "WebDriver:Forward": GeckoDriver.prototype.goForward,
  "WebDriver:FullscreenWindow": GeckoDriver.prototype.fullscreenWindow,
  "WebDriver:GetActiveElement": GeckoDriver.prototype.getActiveElement,
  "WebDriver:GetAlertText": GeckoDriver.prototype.getTextFromDialog,
  "WebDriver:GetCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "WebDriver:GetCookies": GeckoDriver.prototype.getCookies,
  "WebDriver:GetCurrentURL": GeckoDriver.prototype.getCurrentUrl,
  "WebDriver:GetElementAttribute": GeckoDriver.prototype.getElementAttribute,
  "WebDriver:GetElementCSSValue":
    GeckoDriver.prototype.getElementValueOfCssProperty,
  "WebDriver:GetElementProperty": GeckoDriver.prototype.getElementProperty,
  "WebDriver:GetElementRect": GeckoDriver.prototype.getElementRect,
  "WebDriver:GetElementTagName": GeckoDriver.prototype.getElementTagName,
  "WebDriver:GetElementText": GeckoDriver.prototype.getElementText,
  "WebDriver:GetPageSource": GeckoDriver.prototype.getPageSource,
  "WebDriver:GetShadowRoot": GeckoDriver.prototype.getShadowRoot,
  "WebDriver:GetTimeouts": GeckoDriver.prototype.getTimeouts,
  "WebDriver:GetTitle": GeckoDriver.prototype.getTitle,
  "WebDriver:GetWindowHandle": GeckoDriver.prototype.getWindowHandle,
  "WebDriver:GetWindowHandles": GeckoDriver.prototype.getWindowHandles,
  "WebDriver:GetWindowRect": GeckoDriver.prototype.getWindowRect,
  "WebDriver:IsElementDisplayed": GeckoDriver.prototype.isElementDisplayed,
  "WebDriver:IsElementEnabled": GeckoDriver.prototype.isElementEnabled,
  "WebDriver:IsElementSelected": GeckoDriver.prototype.isElementSelected,
  "WebDriver:MinimizeWindow": GeckoDriver.prototype.minimizeWindow,
  "WebDriver:MaximizeWindow": GeckoDriver.prototype.maximizeWindow,
  "WebDriver:Navigate": GeckoDriver.prototype.navigateTo,
  "WebDriver:NewSession": GeckoDriver.prototype.newSession,
  "WebDriver:NewWindow": GeckoDriver.prototype.newWindow,
  "WebDriver:PerformActions": GeckoDriver.prototype.performActions,
  "WebDriver:Print": GeckoDriver.prototype.print,
  "WebDriver:Refresh": GeckoDriver.prototype.refresh,
  "WebDriver:ReleaseActions": GeckoDriver.prototype.releaseActions,
  "WebDriver:SendAlertText": GeckoDriver.prototype.sendKeysToDialog,
  "WebDriver:SetPermission": GeckoDriver.prototype.setPermission,
  "WebDriver:SetTimeouts": GeckoDriver.prototype.setTimeouts,
  "WebDriver:SetWindowRect": GeckoDriver.prototype.setWindowRect,
  "WebDriver:SwitchToFrame": GeckoDriver.prototype.switchToFrame,
  "WebDriver:SwitchToParentFrame": GeckoDriver.prototype.switchToParentFrame,
  "WebDriver:SwitchToWindow": GeckoDriver.prototype.switchToWindow,
  "WebDriver:TakeScreenshot": GeckoDriver.prototype.takeScreenshot,
};

async function exitFullscreen(win) {
  let cb;
  // Use a timed promise to abort if no window manager is present
  await new TimedPromise(
    resolve => {
      cb = new DebounceCallback(resolve);
      win.addEventListener("sizemodechange", cb);
      win.fullScreen = false;
    },
    { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
  win.removeEventListener("sizemodechange", cb);
}

async function restoreWindow(win) {
  win.restore();
  // Use a poll promise to abort if no window manager is present
  await new PollPromise(
    (resolve, reject) => {
      if (WindowState.from(win.windowState) == WindowState.Normal) {
        resolve();
      } else {
        reject();
      }
    },
    { timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
}
