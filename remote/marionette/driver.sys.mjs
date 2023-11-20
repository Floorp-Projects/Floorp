/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Addon: "chrome://remote/content/marionette/addon.sys.mjs",
  AppInfo: "chrome://remote/content/shared/AppInfo.sys.mjs",
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  browser: "chrome://remote/content/marionette/browser.sys.mjs",
  capture: "chrome://remote/content/shared/Capture.sys.mjs",
  Context: "chrome://remote/content/marionette/browser.sys.mjs",
  cookie: "chrome://remote/content/marionette/cookie.sys.mjs",
  DebounceCallback: "chrome://remote/content/marionette/sync.sys.mjs",
  disableEventsActor:
    "chrome://remote/content/marionette/actors/MarionetteEventsParent.sys.mjs",
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  enableEventsActor:
    "chrome://remote/content/marionette/actors/MarionetteEventsParent.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  EventPromise: "chrome://remote/content/shared/Sync.sys.mjs",
  getMarionetteCommandsActorProxy:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs",
  IdlePromise: "chrome://remote/content/marionette/sync.sys.mjs",
  l10n: "chrome://remote/content/marionette/l10n.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  Marionette: "chrome://remote/content/components/Marionette.sys.mjs",
  MarionettePrefs: "chrome://remote/content/marionette/prefs.sys.mjs",
  modal: "chrome://remote/content/shared/Prompt.sys.mjs",
  navigate: "chrome://remote/content/marionette/navigate.sys.mjs",
  permissions: "chrome://remote/content/marionette/permissions.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  print: "chrome://remote/content/shared/PDF.sys.mjs",
  PromptListener:
    "chrome://remote/content/shared/listeners/PromptListener.sys.mjs",
  quit: "chrome://remote/content/shared/Browser.sys.mjs",
  reftest: "chrome://remote/content/marionette/reftest.sys.mjs",
  registerCommandsActor:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs",
  RemoteAgent: "chrome://remote/content/components/RemoteAgent.sys.mjs",
  ShadowRoot: "chrome://remote/content/marionette/web-reference.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  TimedPromise: "chrome://remote/content/marionette/sync.sys.mjs",
  Timeouts: "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
  UnhandledPromptBehavior:
    "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs",
  unregisterCommandsActor:
    "chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs",
  waitForInitialNavigationCompleted:
    "chrome://remote/content/shared/Navigate.sys.mjs",
  webauthn: "chrome://remote/content/marionette/webauthn.sys.mjs",
  WebDriverSession: "chrome://remote/content/shared/webdriver/Session.sys.mjs",
  WebElement: "chrome://remote/content/marionette/web-reference.sys.mjs",
  windowManager: "chrome://remote/content/shared/WindowManager.sys.mjs",
  WindowState: "chrome://remote/content/marionette/browser.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

ChromeUtils.defineLazyGetter(
  lazy,
  "supportedStrategies",
  () =>
    new Set([
      lazy.dom.Strategy.ClassName,
      lazy.dom.Strategy.Selector,
      lazy.dom.Strategy.ID,
      lazy.dom.Strategy.Name,
      lazy.dom.Strategy.LinkText,
      lazy.dom.Strategy.PartialLinkText,
      lazy.dom.Strategy.TagName,
      lazy.dom.Strategy.XPath,
    ])
);

// Timeout used to abort fullscreen, maximize, and minimize
// commands if no window manager is present.
const TIMEOUT_NO_WINDOW_MANAGER = 5000;

// Observer topic to wait for until the browser window is ready.
const TOPIC_BROWSER_READY = "browser-delayed-startup-finished";
// Observer topic to perform clean up when application quit is requested.
const TOPIC_QUIT_APPLICATION_REQUESTED = "quit-application-requested";

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
export function GeckoDriver(server) {
  this._server = server;

  // WebDriver Session
  this._currentSession = null;

  this.browsers = {};

  // points to current browser
  this.curBrowser = null;
  // top-most chrome window
  this.mainFrame = null;

  // Use content context by default
  this.context = lazy.Context.Content;

  // used for modal dialogs
  this.dialog = null;
  this.promptListener = null;
}

/**
 * The current context decides if commands are executed in chrome- or
 * content space.
 */
Object.defineProperty(GeckoDriver.prototype, "context", {
  get() {
    return this._context;
  },

  set(context) {
    this._context = lazy.Context.fromString(context);
  },
});

/**
 * The current WebDriver Session.
 */
Object.defineProperty(GeckoDriver.prototype, "currentSession", {
  get() {
    if (lazy.RemoteAgent.webDriverBiDi) {
      return lazy.RemoteAgent.webDriverBiDi.session;
    }

    return this._currentSession;
  },
});

/**
 * Returns the current URL of the ChromeWindow or content browser,
 * depending on context.
 *
 * @returns {URL}
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
 * @returns {string}
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
 * Callback used to observe the closing of modal dialogs
 * during the session's lifetime.
 */
GeckoDriver.prototype.handleClosedModalDialog = function () {
  this.dialog = null;
};

/**
 * Callback used to observe the creation of new modal dialogs
 * during the session's lifetime.
 */
GeckoDriver.prototype.handleOpenModalDialog = function (eventName, data) {
  this.dialog = data.prompt;
  this.getActor().notifyDialogOpened();
};

/**
 * Get the current visible URL.
 */
GeckoDriver.prototype._getCurrentURL = function () {
  const browsingContext = this.getBrowsingContext({ top: true });
  return new URL(browsingContext.currentURI.spec);
};

/**
 * Get the current "MarionetteCommands" parent actor.
 *
 * @param {object} options
 * @param {boolean=} options.top
 *     If set to true use the window's top-level browsing context for the actor,
 *     otherwise the one from the currently selected frame. Defaults to false.
 *
 * @returns {MarionetteCommandsParent}
 *     The parent actor.
 */
GeckoDriver.prototype.getActor = function (options = {}) {
  return lazy.getMarionetteCommandsActorProxy(() =>
    this.getBrowsingContext(options)
  );
};

/**
 * Get the selected BrowsingContext for the current context.
 *
 * @param {object} options
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
 * @returns {BrowsingContext}
 *     The browsing context, or `null` if none is available
 */
GeckoDriver.prototype.getBrowsingContext = function (options = {}) {
  const { context = this.context, parent = false, top = false } = options;

  let browsingContext = null;
  if (context === lazy.Context.Chrome) {
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
 * @param {object} options
 * @param {Context=} options.context
 *     Optional name of the context to use for finding the window.
 *     It will be required if a command always needs a specific context,
 *     whether which context is currently set. Defaults to the current
 *     context.
 *
 * @returns {ChromeWindow}
 *     The current top-level browsing context.
 */
GeckoDriver.prototype.getCurrentWindow = function (options = {}) {
  const { context = this.context } = options;

  let win = null;
  switch (context) {
    case lazy.Context.Chrome:
      if (this.curBrowser) {
        win = this.curBrowser.window;
      }
      break;

    case lazy.Context.Content:
      if (this.curBrowser && this.curBrowser.contentBrowser) {
        win = this.curBrowser.window;
      }
      break;
  }

  return win;
};

GeckoDriver.prototype.isReftestBrowser = function (element) {
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
 * @returns {string}
 *     Returns the unique server-assigned ID of the window.
 */
GeckoDriver.prototype.addBrowser = function (win) {
  let context = new lazy.browser.Context(win, this);
  let winId = lazy.windowManager.getIdForWindow(win);

  this.browsers[winId] = context;
  this.curBrowser = this.browsers[winId];
};

/**
 * Handles registration of new content browsers.  Depending on
 * their type they are either accepted or ignored.
 *
 * @param {XULBrowser} browserElement
 */
GeckoDriver.prototype.registerBrowser = function (browserElement) {
  // We want to ignore frames that are XUL browsers that aren't in the "main"
  // tabbrowser, but accept things on Fennec (which doesn't have a
  // xul:tabbrowser), and accept HTML iframes (because tests depend on it),
  // as well as XUL frames. Ideally this should be cleaned up and we should
  // keep track of browsers a different way.
  if (
    !lazy.AppInfo.isFirefox ||
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
 * @param {object} cmd
 * @param {Object<string, *>=} cmd.parameters
 *     JSON Object containing any of the recognised capabilities as listed
 *     on the `WebDriverSession` class.
 *
 * @returns {object}
 *     Session ID and capabilities offered by the WebDriver service.
 *
 * @throws {SessionNotCreatedError}
 *     If, for whatever reason, a session could not be created.
 */
GeckoDriver.prototype.newSession = async function (cmd) {
  if (this.currentSession) {
    throw new lazy.error.SessionNotCreatedError(
      "Maximum number of active sessions"
    );
  }

  const { parameters: capabilities } = cmd;

  try {
    // If the WebDriver BiDi protocol is active always use the Remote Agent
    // to handle the WebDriver session. If it's not the case then Marionette
    // itself needs to handle it, and has to nullify the "webSocketUrl"
    // capability.
    if (lazy.RemoteAgent.webDriverBiDi) {
      await lazy.RemoteAgent.webDriverBiDi.createSession(capabilities);
    } else {
      this._currentSession = new lazy.WebDriverSession(capabilities);
      this._currentSession.capabilities.delete("webSocketUrl");
    }

    // Don't wait for the initial window when Marionette is in windowless mode
    if (!this.currentSession.capabilities.get("moz:windowless")) {
      // Creating a WebDriver session too early can cause issues with
      // clients in not being able to find any available window handle.
      // Also when closing the application while it's still starting up can
      // cause shutdown hangs. As such Marionette will return a new session
      // once the initial application window has finished initializing.
      lazy.logger.debug(`Waiting for initial application window`);
      await lazy.Marionette.browserStartupFinished;

      const appWin =
        await lazy.windowManager.waitForInitialApplicationWindowLoaded();

      if (lazy.MarionettePrefs.clickToStart) {
        Services.prompt.alert(
          appWin,
          "",
          "Click to start execution of marionette tests"
        );
      }

      this.addBrowser(appWin);
      this.mainFrame = appWin;

      // Setup observer for modal dialogs
      this.promptListener = new lazy.PromptListener(() => this.curBrowser);
      this.promptListener.on("closed", this.handleClosedModalDialog.bind(this));
      this.promptListener.on("opened", this.handleOpenModalDialog.bind(this));
      this.promptListener.startListening();

      for (let win of lazy.windowManager.windows) {
        this.registerWindow(win, { registerBrowsers: true });
      }

      if (this.mainFrame) {
        this.currentSession.chromeBrowsingContext =
          this.mainFrame.browsingContext;
        this.mainFrame.focus();
      }

      if (this.curBrowser.tab) {
        const browsingContext = this.curBrowser.contentBrowser.browsingContext;
        this.currentSession.contentBrowsingContext = browsingContext;

        // Bug 1838381 - Only use a longer unload timeout for desktop, because
        // on Android only the initial document is loaded, and loading a
        // specific page during startup doesn't succeed.
        const options = {};
        if (!lazy.AppInfo.isAndroid) {
          options.unloadTimeout = 5000;
        }

        await lazy.waitForInitialNavigationCompleted(
          browsingContext.webProgress,
          options
        );

        this.curBrowser.contentBrowser.focus();
      }

      // Check if there is already an open dialog for the selected browser window.
      this.dialog = lazy.modal.findPrompt(this.curBrowser);
    }

    lazy.registerCommandsActor(this.currentSession.id);
    lazy.enableEventsActor();

    Services.obs.addObserver(this, TOPIC_BROWSER_READY);
  } catch (e) {
    throw new lazy.error.SessionNotCreatedError(e);
  }

  return {
    sessionId: this.currentSession.id,
    capabilities: this.currentSession.capabilities,
  };
};

/**
 * Start observing the specified window.
 *
 * @param {ChromeWindow} win
 *     Chrome window to register event listeners for.
 * @param {object=} options
 * @param {boolean=} options.registerBrowsers
 *     If true, register all content browsers of found tabs. Defaults to false.
 */
GeckoDriver.prototype.registerWindow = function (win, options = {}) {
  const { registerBrowsers = false } = options;
  const tabBrowser = lazy.TabManager.getTabBrowser(win);

  if (registerBrowsers && tabBrowser) {
    for (const tab of tabBrowser.tabs) {
      const contentBrowser = lazy.TabManager.getBrowserForTab(tab);
      this.registerBrowser(contentBrowser);
    }
  }

  // Listen for any kind of top-level process switch
  tabBrowser?.addEventListener("XULFrameLoaderCreated", this);
};

/**
 * Stop observing the specified window.
 *
 * @param {ChromeWindow} win
 *     Chrome window to unregister event listeners for.
 */
GeckoDriver.prototype.stopObservingWindow = function (win) {
  const tabBrowser = lazy.TabManager.getTabBrowser(win);

  tabBrowser?.removeEventListener("XULFrameLoaderCreated", this);
};

GeckoDriver.prototype.handleEvent = function ({ target, type }) {
  switch (type) {
    case "XULFrameLoaderCreated":
      if (target === this.curBrowser.contentBrowser) {
        lazy.logger.trace(
          "Remoteness change detected. Set new top-level browsing context " +
            `to ${target.browsingContext.id}`
        );

        this.currentSession.contentBrowsingContext = target.browsingContext;
      }
      break;
  }
};

GeckoDriver.prototype.observe = async function (subject, topic, data) {
  switch (topic) {
    case TOPIC_BROWSER_READY:
      this.registerWindow(subject);
      break;

    case TOPIC_QUIT_APPLICATION_REQUESTED:
      // Run Marionette specific cleanup steps before allowing
      // the application to shutdown
      await this._server.setAcceptConnections(false);
      this.deleteSession();
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
GeckoDriver.prototype.getSessionCapabilities = function () {
  return { capabilities: this.currentSession.capabilities };
};

/**
 * Sets the context of the subsequent commands.
 *
 * All subsequent requests to commands that in some way involve
 * interaction with a browsing context will target the chosen browsing
 * context.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.value
 *     Name of the context to be switched to.  Must be one of "chrome" or
 *     "content".
 *
 * @throws {InvalidArgumentError}
 *     If <var>value</var> is not a string.
 * @throws {WebDriverError}
 *     If <var>value</var> is not a valid browsing context.
 */
GeckoDriver.prototype.setContext = function (cmd) {
  let value = lazy.assert.string(cmd.parameters.value);

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
 * @returns {Context}
 *     Current context.
 */
GeckoDriver.prototype.getContext = function () {
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
 * @param {object} cmd
 * @param {string} cmd.parameters.script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebReference)>} cmd.parameters.args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {string=} cmd.parameters.sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If he parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} cmd.parameters.newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} cmd.parameters.filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} cmd.parameters.line
 *     Line in the client's program where this script is evaluated.
 *
 * @returns {(string|boolean|number|object|WebReference)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {JavaScriptError}
 *     If an {@link Error} was thrown whilst evaluating the script.
 * @throws {NoSuchElementError}
 *     If an element that was passed as part of <var>args</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 * @throws {StaleElementReferenceError}
 *     If an element that was passed as part of <var>args</var> or that is
 *     returned as result has gone stale.
 */
GeckoDriver.prototype.executeScript = function (cmd) {
  let { script, args } = cmd.parameters;
  let opts = {
    script: cmd.parameters.script,
    args: cmd.parameters.args,
    sandboxName: cmd.parameters.sandbox,
    newSandbox: cmd.parameters.newSandbox,
    file: cmd.parameters.filename,
    line: cmd.parameters.line,
  };

  return this.execute_(script, args, opts);
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
 * @param {object} cmd
 * @param {string} cmd.parameters.script
 *     Script to evaluate as a function body.
 * @param {Array.<(string|boolean|number|object|WebReference)>} cmd.parameters.args
 *     Arguments exposed to the script in <code>arguments</code>.
 *     The array items must be serialisable to the WebDriver protocol.
 * @param {string=} cmd.parameters.sandbox
 *     Name of the sandbox to evaluate the script in.  The sandbox is
 *     cached for later re-use on the same Window object if
 *     <var>newSandbox</var> is false.  If the parameter is undefined,
 *     the script is evaluated in a mutable sandbox.  If the parameter
 *     is "system", it will be evaluted in a sandbox with elevated system
 *     privileges, equivalent to chrome space.
 * @param {boolean=} cmd.parameters.newSandbox
 *     Forces the script to be evaluated in a fresh sandbox.  Note that if
 *     it is undefined, the script will normally be evaluted in a fresh
 *     sandbox.
 * @param {string=} cmd.parameters.filename
 *     Filename of the client's program where this script is evaluated.
 * @param {number=} cmd.parameters.line
 *     Line in the client's program where this script is evaluated.
 *
 * @returns {(string|boolean|number|object|WebReference)}
 *     Return value from the script, or null which signifies either the
 *     JavaScript notion of null or undefined.
 *
 * @throws {JavaScriptError}
 *     If an Error was thrown whilst evaluating the script.
 * @throws {NoSuchElementError}
 *     If an element that was passed as part of <var>args</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {ScriptTimeoutError}
 *     If the script was interrupted due to reaching the session's
 *     script timeout.
 * @throws {StaleElementReferenceError}
 *     If an element that was passed as part of <var>args</var> or that is
 *     returned as result has gone stale.
 */
GeckoDriver.prototype.executeAsyncScript = function (cmd) {
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

  return this.execute_(script, args, opts);
};

GeckoDriver.prototype.execute_ = async function (
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
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  lazy.assert.string(
    script,
    lazy.pprint`Expected "script" to be a string: ${script}`
  );
  lazy.assert.array(
    args,
    lazy.pprint`Expected script args to be an array: ${args}`
  );
  if (sandboxName !== null) {
    lazy.assert.string(
      sandboxName,
      lazy.pprint`Expected sandbox name to be a string: ${sandboxName}`
    );
  }
  lazy.assert.boolean(
    newSandbox,
    lazy.pprint`Expected newSandbox to be boolean: ${newSandbox}`
  );
  lazy.assert.string(file, lazy.pprint`Expected file to be a string: ${file}`);
  lazy.assert.number(line, lazy.pprint`Expected line to be a number: ${line}`);

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
 * @param {object} cmd
 * @param {string} cmd.parameters.url
 *     URL to navigate to.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in current context.
 */
GeckoDriver.prototype.navigateTo = async function (cmd) {
  lazy.assert.content(this.context);
  const browsingContext = lazy.assert.open(
    this.getBrowsingContext({ top: true })
  );
  await this._handleUserPrompts();

  let validURL;
  try {
    validURL = new URL(cmd.parameters.url);
  } catch (e) {
    throw new lazy.error.InvalidArgumentError(`Malformed URL: ${e.message}`);
  }

  // Switch to the top-level browsing context before navigating
  this.currentSession.contentBrowsingContext = browsingContext;

  const loadEventExpected = lazy.navigate.isLoadEventExpected(
    this._getCurrentURL(),
    {
      future: validURL,
    }
  );

  await lazy.navigate.waitForNavigationCompleted(
    this,
    () => {
      lazy.navigate.navigateTo(browsingContext, validURL);
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
GeckoDriver.prototype.getCurrentUrl = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  return this._getCurrentURL().href;
};

/**
 * Gets the current title of the window.
 *
 * @returns {string}
 *     Document title of the top-level browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getTitle = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  return this.title;
};

/**
 * Gets the current type of the window.
 *
 * @returns {string}
 *     Type of window
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getWindowType = function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));

  return this.windowType;
};

/**
 * Gets the page source of the content document.
 *
 * @returns {string}
 *     String serialisation of the DOM of the current browsing context's
 *     active document.
 *
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getPageSource = async function () {
  lazy.assert.open(this.getBrowsingContext());
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
GeckoDriver.prototype.goBack = async function () {
  lazy.assert.content(this.context);
  const browsingContext = lazy.assert.open(
    this.getBrowsingContext({ top: true })
  );
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!browsingContext.embedderElement?.canGoBack) {
    return;
  }

  await lazy.navigate.waitForNavigationCompleted(this, () => {
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
GeckoDriver.prototype.goForward = async function () {
  lazy.assert.content(this.context);
  const browsingContext = lazy.assert.open(
    this.getBrowsingContext({ top: true })
  );
  await this._handleUserPrompts();

  // If there is no history, just return
  if (!browsingContext.embedderElement?.canGoForward) {
    return;
  }

  await lazy.navigate.waitForNavigationCompleted(this, () => {
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
GeckoDriver.prototype.refresh = async function () {
  lazy.assert.content(this.context);
  const browsingContext = lazy.assert.open(
    this.getBrowsingContext({ top: true })
  );
  await this._handleUserPrompts();

  // Switch to the top-level browsing context before navigating
  this.currentSession.contentBrowsingContext = browsingContext;

  await lazy.navigate.waitForNavigationCompleted(this, () => {
    lazy.navigate.refresh(browsingContext);
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
 * @returns {string}
 *     Unique window handle.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getWindowHandle = function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));

  if (this.context == lazy.Context.Chrome) {
    return lazy.windowManager.getIdForWindow(this.curBrowser.window);
  }
  return lazy.TabManager.getIdForBrowser(this.curBrowser.contentBrowser);
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
 * @returns {Array.<string>}
 *     Unique window handles.
 */
GeckoDriver.prototype.getWindowHandles = function () {
  if (this.context == lazy.Context.Chrome) {
    return lazy.windowManager.chromeWindowHandles.map(String);
  }
  return lazy.TabManager.allBrowserUniqueIds.map(String);
};

/**
 * Get the current position and size of the browser window currently in focus.
 *
 * Will return the current browser window size in pixels. Refers to
 * window outerWidth and outerHeight values, which include scroll bars,
 * title bars, etc.
 *
 * @returns {Object<string, number>}
 *     Object with |x| and |y| coordinates, and |width| and |height|
 *     of browser window.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getWindowRect = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
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
 * @param {object} cmd
 * @param {number} cmd.parameters.x
 *     X coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} cmd.parameters.y
 *     Y coordinate of the top/left of the window that it will be
 *     moved to.
 * @param {number} cmd.parameters.width
 *     Width to resize the window to.
 * @param {number} cmd.parameters.height
 *     Height to resize the window to.
 *
 * @returns {Object<string, number>}
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
GeckoDriver.prototype.setWindowRect = async function (cmd) {
  lazy.assert.desktop();
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const { x = null, y = null, width = null, height = null } = cmd.parameters;
  if (x !== null) {
    lazy.assert.integer(x);
  }
  if (y !== null) {
    lazy.assert.integer(y);
  }
  if (height !== null) {
    lazy.assert.positiveInteger(height);
  }
  if (width !== null) {
    lazy.assert.positiveInteger(width);
  }

  const win = this.getCurrentWindow();
  switch (lazy.WindowState.from(win.windowState)) {
    case lazy.WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case lazy.WindowState.Maximized:
    case lazy.WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  function geometryMatches() {
    if (
      width !== null &&
      height !== null &&
      (win.outerWidth !== width || win.outerHeight !== height)
    ) {
      return false;
    }
    if (x !== null && y !== null && (win.screenX !== x || win.screenY !== y)) {
      return false;
    }
    lazy.logger.trace(`Requested window geometry matches`);
    return true;
  }

  if (!geometryMatches()) {
    // There might be more than one resize or MozUpdateWindowPos event due
    // to previous geometry changes, such as from restoreWindow(), so
    // wait longer if window geometry does not match.
    const options = { checkFn: geometryMatches, timeout: 500 };
    const promises = [];
    if (width !== null && height !== null) {
      promises.push(new lazy.EventPromise(win, "resize", options));
      win.resizeTo(width, height);
    }
    if (x !== null && y !== null) {
      promises.push(
        new lazy.EventPromise(win.windowRoot, "MozUpdateWindowPos", options)
      );
      win.moveTo(x, y);
    }
    try {
      await Promise.race(promises);
    } catch (e) {
      if (e instanceof lazy.error.TimeoutError) {
        // The operating system might not honor the move or resize, in which
        // case assume that geometry will have been adjusted "as close as
        // possible" to that requested.  There may be no event received if the
        // geometry is already as close as possible.
      } else {
        throw e;
      }
    }
  }

  return this.curBrowser.rect;
};

/**
 * Switch current top-level browsing context by name or server-assigned
 * ID.  Searches for windows by name, then ID.  Content windows take
 * precedence.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.handle
 *     Handle of the window to switch to.
 * @param {boolean=} cmd.parameters.focus
 *     A boolean value which determines whether to focus
 *     the window. Defaults to true.
 *
 * @throws {InvalidArgumentError}
 *     If <var>handle</var> is not a string or <var>focus</var> not a boolean.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.switchToWindow = async function (cmd) {
  const { focus = true, handle } = cmd.parameters;

  lazy.assert.string(
    handle,
    lazy.pprint`Expected "handle" to be a string, got ${handle}`
  );
  lazy.assert.boolean(
    focus,
    lazy.pprint`Expected "focus" to be a boolean, got ${focus}`
  );

  const found = lazy.windowManager.findWindowByHandle(handle);

  let selected = false;
  if (found) {
    try {
      await this.setWindowHandle(found, focus);
      selected = true;
    } catch (e) {
      lazy.logger.error(e);
    }
  }

  if (!selected) {
    throw new lazy.error.NoSuchWindowError(
      `Unable to locate window: ${handle}`
    );
  }
};

/**
 * Switch the marionette window to a given window. If the browser in
 * the window is unregistered, register that browser and wait for
 * the registration is complete. If |focus| is true then set the focus
 * on the window.
 *
 * @param {object} winProperties
 *     Object containing window properties such as returned from
 *     :js:func:`GeckoDriver#getWindowProperties`
 * @param {boolean=} focus
 *     A boolean value which determines whether to focus the window.
 *     Defaults to true.
 */
GeckoDriver.prototype.setWindowHandle = async function (
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
      const tabBrowser = lazy.TabManager.getTabBrowser(winProperties.win);

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

  // Check for an existing dialog for the new window
  this.dialog = lazy.modal.findPrompt(this.curBrowser);

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
GeckoDriver.prototype.switchToParentFrame = async function () {
  let browsingContext = this.getBrowsingContext();
  if (browsingContext && !browsingContext.parent) {
    return;
  }

  browsingContext = lazy.assert.open(browsingContext?.parent);

  this.currentSession.contentBrowsingContext = browsingContext;
};

/**
 * Switch to a given frame within the current window.
 *
 * @param {object} cmd
 * @param {(string | object)=} cmd.parameters.element
 *     A web element reference of the frame or its element id.
 * @param {number=} cmd.parameters.id
 *     The index of the frame to switch to.
 *     If both element and id are not defined, switch to top-level frame.
 *
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>element</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>element</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.switchToFrame = async function (cmd) {
  const { element: el, id } = cmd.parameters;

  if (typeof id == "number") {
    lazy.assert.unsignedShort(
      id,
      `Expected id to be unsigned short, got ${id}`
    );
  }

  const top = id == null && el == null;
  lazy.assert.open(this.getBrowsingContext({ top }));
  await this._handleUserPrompts();

  // Bug 1495063: Elements should be passed as WebReference reference
  let byFrame;
  if (typeof el == "string") {
    byFrame = lazy.WebElement.fromUUID(el).toJSON();
  } else if (el) {
    byFrame = el;
  }

  const { browsingContext } = await this.getActor({ top }).switchToFrame(
    byFrame || id
  );

  this.currentSession.contentBrowsingContext = browsingContext;
};

GeckoDriver.prototype.getTimeouts = function () {
  return this.currentSession.timeouts;
};

/**
 * Set timeout for page loading, searching, and scripts.
 *
 * @param {object} cmd
 * @param {Object<string, number>} cmd.parameters
 *     Dictionary of timeout types and their new value, where all timeout
 *     types are optional.
 *
 * @throws {InvalidArgumentError}
 *     If timeout type key is unknown, or the value provided with it is
 *     not an integer.
 */
GeckoDriver.prototype.setTimeouts = function (cmd) {
  // merge with existing timeouts
  let merged = Object.assign(
    this.currentSession.timeouts.toJSON(),
    cmd.parameters
  );

  this.currentSession.timeouts = lazy.Timeouts.fromJSON(merged);
};

/**
 * Perform a series of grouped actions at the specified points in time.
 *
 * @param {object} cmd
 * @param {Array<?>} cmd.parameters.actions
 *     Array of objects that each represent an action sequence.
 *
 * @throws {NoSuchElementError}
 *     If an element that is used as part of the action chain is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If an element that is used as part of the action chain has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not yet available in current context.
 */
GeckoDriver.prototype.performActions = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const actions = cmd.parameters.actions;
  await this.getActor().performActions(actions);
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
GeckoDriver.prototype.releaseActions = async function () {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  await this.getActor().releaseActions();
};

/**
 * Find an element using the indicated search strategy.
 *
 * @param {object} cmd
 * @param {string=} cmd.parameters.element
 *     Web element reference ID to the element that will be used as start node.
 * @param {string} cmd.parameters.using
 *     Indicates which search method to use.
 * @param {string} cmd.parameters.value
 *     Value the client is looking for.
 *
 * @returns {WebElement}
 *     Return the found element.
 *
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>element</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>element</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElement = async function (cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!lazy.supportedStrategies.has(using)) {
    throw new lazy.error.InvalidSelectorError(
      `Strategy not supported: ${using}`
    );
  }

  lazy.assert.defined(value);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = lazy.WebElement.fromUUID(el).toJSON();
  }

  let opts = {
    startNode,
    timeout: this.currentSession.timeouts.implicit,
    all: false,
  };

  return this.getActor().findElement(using, value, opts);
};

/**
 * Find an element within shadow root using the indicated search strategy.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.shadowRoot
 *     Shadow root reference ID.
 * @param {string} cmd.parameters.using
 *     Indicates which search method to use.
 * @param {string} cmd.parameters.value
 *     Value the client is looking for.
 *
 * @returns {WebElement}
 *     Return the found element.
 *
 * @throws {DetachedShadowRootError}
 *     If shadow root represented by reference <var>id</var> is
 *     no longer attached to the DOM.
 * @throws {NoSuchElementError}
 *     If the element which is looked for with <var>value</var> was
 *     not found.
 * @throws {NoSuchShadowRoot}
 *     If shadow root represented by reference <var>shadowRoot</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElementFromShadowRoot = async function (cmd) {
  const { shadowRoot, using, value } = cmd.parameters;

  if (!lazy.supportedStrategies.has(using)) {
    throw new lazy.error.InvalidSelectorError(
      `Strategy not supported: ${using}`
    );
  }

  lazy.assert.defined(value);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const opts = {
    all: false,
    startNode: lazy.ShadowRoot.fromUUID(shadowRoot).toJSON(),
    timeout: this.currentSession.timeouts.implicit,
  };

  return this.getActor().findElement(using, value, opts);
};

/**
 * Find elements using the indicated search strategy.
 *
 * @param {object} cmd
 * @param {string=} cmd.parameters.element
 *     Web element reference ID to the element that will be used as start node.
 * @param {string} cmd.parameters.using
 *     Indicates which search method to use.
 * @param {string} cmd.parameters.value
 *     Value the client is looking for.
 *
 * @returns {Array<WebElement>}
 *     Return the array of found elements.
 *
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>element</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>element</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElements = async function (cmd) {
  const { element: el, using, value } = cmd.parameters;

  if (!lazy.supportedStrategies.has(using)) {
    throw new lazy.error.InvalidSelectorError(
      `Strategy not supported: ${using}`
    );
  }

  lazy.assert.defined(value);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let startNode;
  if (typeof el != "undefined") {
    startNode = lazy.WebElement.fromUUID(el).toJSON();
  }

  let opts = {
    startNode,
    timeout: this.currentSession.timeouts.implicit,
    all: true,
  };

  return this.getActor().findElements(using, value, opts);
};

/**
 * Find elements within shadow root using the indicated search strategy.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.shadowRoot
 *     Shadow root reference ID.
 * @param {string} cmd.parameters.using
 *     Indicates which search method to use.
 * @param {string} cmd.parameters.value
 *     Value the client is looking for.
 *
 * @returns {Array<WebElement>}
 *     Return the array of found elements.
 *
 * @throws {DetachedShadowRootError}
 *     If shadow root represented by reference <var>id</var> is
 *     no longer attached to the DOM.
 * @throws {NoSuchShadowRoot}
 *     If shadow root represented by reference <var>shadowRoot</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.findElementsFromShadowRoot = async function (cmd) {
  const { shadowRoot, using, value } = cmd.parameters;

  if (!lazy.supportedStrategies.has(using)) {
    throw new lazy.error.InvalidSelectorError(
      `Strategy not supported: ${using}`
    );
  }

  lazy.assert.defined(value);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const opts = {
    all: true,
    startNode: lazy.ShadowRoot.fromUUID(shadowRoot).toJSON(),
    timeout: this.currentSession.timeouts.implicit,
  };

  return this.getActor().findElements(using, value, opts);
};

/**
 * Return the shadow root of an element in the document.
 *
 * @param {object} cmd
 * @param {id} cmd.parameters.id
 *     A web element id reference.
 * @returns {ShadowRoot}
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
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in chrome current context.
 */
GeckoDriver.prototype.getShadowRoot = async function (cmd) {
  // Bug 1743541: Add support for chrome scope.
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(
    cmd.parameters.id,
    lazy.pprint`Expected "id" to be a string, got ${cmd.parameters.id}`
  );
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getShadowRoot(webEl);
};

/**
 * Return the active element in the document.
 *
 * @returns {WebReference}
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
 *     Not available in chrome context.
 */
GeckoDriver.prototype.getActiveElement = async function () {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  return this.getActor().getActiveElement();
};

/**
 * Send click event to element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be clicked.
 *
 * @throws {InvalidArgumentError}
 *     If element <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clickElement = async function (cmd) {
  const browsingContext = lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  const actor = this.getActor();

  const loadEventExpected = lazy.navigate.isLoadEventExpected(
    this._getCurrentURL(),
    {
      browsingContext,
      target: await actor.getElementAttribute(webEl, "target"),
    }
  );

  await lazy.navigate.waitForNavigationCompleted(
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
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} cmd.parameters.name
 *     Name of the attribute which value to retrieve.
 *
 * @returns {string}
 *     Value of the attribute.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementAttribute = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const id = lazy.assert.string(cmd.parameters.id);
  const name = lazy.assert.string(cmd.parameters.name);
  const webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementAttribute(webEl, name);
};

/**
 * Returns the value of a property associated with given element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Web element reference ID to the element that will be inspected.
 * @param {string} cmd.parameters.name
 *     Name of the property which value to retrieve.
 *
 * @returns {string}
 *     Value of the property.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>name</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementProperty = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  const id = lazy.assert.string(cmd.parameters.id);
  const name = lazy.assert.string(cmd.parameters.name);
  const webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementProperty(webEl, name);
};

/**
 * Get the text of an element, if any.  Includes the text of all child
 * elements.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be inspected.
 *
 * @returns {string}
 *     Element's text "as rendered".
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementText = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementText(webEl);
};

/**
 * Get the tag name of the element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be inspected.
 *
 * @returns {string}
 *     Local tag name of element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementTagName = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementTagName(webEl);
};

/**
 * Check if element is displayed.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be inspected.
 *
 * @returns {boolean}
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
GeckoDriver.prototype.isElementDisplayed = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().isElementDisplayed(
    webEl,
    this.currentSession.capabilities
  );
};

/**
 * Return the property of the computed style of an element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be checked.
 * @param {string} cmd.parameters.propertyName
 *     CSS rule that is being requested.
 *
 * @returns {string}
 *     Value of |propertyName|.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>propertyName</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementValueOfCssProperty = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let prop = lazy.assert.string(cmd.parameters.propertyName);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementValueOfCssProperty(webEl, prop);
};

/**
 * Check if element is enabled.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be checked.
 *
 * @returns {boolean}
 *     True if enabled, false if disabled.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.isElementEnabled = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().isElementEnabled(
    webEl,
    this.currentSession.capabilities
  );
};

/**
 * Check if element is selected.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be checked.
 *
 * @returns {boolean}
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
GeckoDriver.prototype.isElementSelected = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

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
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.getElementRect = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getElementRect(webEl);
};

/**
 * Send key presses to element after focusing on it.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be checked.
 * @param {string} cmd.parameters.text
 *     Value to send to the element.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> or <var>text</var> are not strings.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.sendKeysToElement = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let text = lazy.assert.string(cmd.parameters.text);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().sendKeysToElement(
    webEl,
    text,
    this.currentSession.capabilities
  );
};

/**
 * Clear the text of an element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Reference ID to the element that will be cleared.
 *
 * @throws {InvalidArgumentError}
 *     If <var>id</var> is not a string.
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.clearElement = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  await this.getActor().clearElement(webEl);
};

/**
 * Add a single cookie to the cookie store associated with the active
 * document's address.
 *
 * @param {object} cmd
 * @param {Map.<string, (string|number|boolean)>} cmd.parameters.cookie
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
GeckoDriver.prototype.addCookie = async function (cmd) {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { protocol, hostname } = this._getCurrentURL();

  const networkSchemes = ["http:", "https:"];
  if (!networkSchemes.includes(protocol)) {
    throw new lazy.error.InvalidCookieDomainError("Document is cookie-averse");
  }

  let newCookie = lazy.cookie.fromJSON(cmd.parameters.cookie);

  lazy.cookie.add(newCookie, { restrictToHost: hostname, protocol });
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
GeckoDriver.prototype.getCookies = async function () {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  return [...lazy.cookie.iter(hostname, pathname)];
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
GeckoDriver.prototype.deleteAllCookies = async function () {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  for (let toDelete of lazy.cookie.iter(hostname, pathname)) {
    lazy.cookie.remove(toDelete);
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
GeckoDriver.prototype.deleteCookie = async function (cmd) {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let { hostname, pathname } = this._getCurrentURL();
  let name = lazy.assert.string(cmd.parameters.name);
  for (let c of lazy.cookie.iter(hostname, pathname)) {
    if (c.name === name) {
      lazy.cookie.remove(c);
    }
  }
};

/**
 * Open a new top-level browsing context.
 *
 * @param {object} cmd
 * @param {string=} cmd.parameters.type
 *     Optional type of the new top-level browsing context. Can be one of
 *     `tab` or `window`. Defaults to `tab`.
 * @param {boolean=} cmd.parameters.focus
 *     Optional flag if the new top-level browsing context should be opened
 *     in foreground (focused) or background (not focused). Defaults to false.
 * @param {boolean=} cmd.parameters.private
 *     Optional flag, which gets only evaluated for type `window`. True if the
 *     new top-level browsing context should be a private window.
 *     Defaults to false.
 *
 * @returns {Object<string, string>}
 *     Handle and type of the new browsing context.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.newWindow = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let focus = false;
  if (typeof cmd.parameters.focus != "undefined") {
    focus = lazy.assert.boolean(
      cmd.parameters.focus,
      lazy.pprint`Expected "focus" to be a boolean, got ${cmd.parameters.focus}`
    );
  }

  let isPrivate = false;
  if (typeof cmd.parameters.private != "undefined") {
    isPrivate = lazy.assert.boolean(
      cmd.parameters.private,
      lazy.pprint`Expected "private" to be a boolean, got ${cmd.parameters.private}`
    );
  }

  let type;
  if (typeof cmd.parameters.type != "undefined") {
    type = lazy.assert.string(
      cmd.parameters.type,
      lazy.pprint`Expected "type" to be a string, got ${cmd.parameters.type}`
    );
  }

  // If an invalid or no type has been specified default to a tab.
  if (typeof type == "undefined" || !["tab", "window"].includes(type)) {
    type = "tab";
  }

  let contentBrowser;

  switch (type) {
    case "window":
      let win = await this.curBrowser.openBrowserWindow(focus, isPrivate);
      contentBrowser = lazy.TabManager.getTabBrowser(win).selectedBrowser;
      break;

    default:
      // To not fail if a new type gets added in the future, make opening
      // a new tab the default action.
      let tab = await this.curBrowser.openTab(focus);
      contentBrowser = lazy.TabManager.getBrowserForTab(tab);
  }

  // Actors need the new window to be loaded to safely execute queries.
  // Wait until the initial page load has been finished.
  await lazy.waitForInitialNavigationCompleted(
    contentBrowser.browsingContext.webProgress,
    {
      unloadTimeout: 5000,
    }
  );

  const id = lazy.TabManager.getIdForBrowser(contentBrowser);

  return { handle: id.toString(), type };
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
 * @returns {Array.<string>}
 *     Unique window handles of remaining windows.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 */
GeckoDriver.prototype.close = async function () {
  lazy.assert.open(
    this.getBrowsingContext({ context: lazy.Context.Content, top: true })
  );
  await this._handleUserPrompts();

  // If there is only one window left, do not close unless windowless mode is
  // enabled. Instead return a faked empty array of window handles.
  // This will instruct geckodriver to terminate the application.
  if (
    lazy.TabManager.getTabCount() === 1 &&
    !this.currentSession.capabilities.get("moz:windowless")
  ) {
    return [];
  }

  await this.curBrowser.closeTab();
  this.currentSession.contentBrowsingContext = null;

  return lazy.TabManager.allBrowserUniqueIds.map(String);
};

/**
 * Close the currently selected chrome window.
 *
 * If it is the last window currently open, the chrome window will not be
 * closed to prevent a shutdown of the application. Instead the returned
 * list of chrome window handles is empty.
 *
 * @returns {Array.<string>}
 *     Unique chrome window handles of remaining chrome windows.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.closeChromeWindow = async function () {
  lazy.assert.desktop();
  lazy.assert.open(
    this.getBrowsingContext({ context: lazy.Context.Chrome, top: true })
  );

  let nwins = 0;

  // eslint-disable-next-line
  for (let _ of lazy.windowManager.windows) {
    nwins++;
  }

  // If there is only one window left, do not close unless windowless mode is
  // enabled. Instead return a faked empty array of window handles.
  // This will instruct geckodriver to terminate the application.
  if (nwins == 1 && !this.currentSession.capabilities.get("moz:windowless")) {
    return [];
  }

  await this.curBrowser.closeWindow();
  this.currentSession.chromeBrowsingContext = null;
  this.currentSession.contentBrowsingContext = null;

  return lazy.windowManager.chromeWindowHandles.map(String);
};

/** Delete Marionette session. */
GeckoDriver.prototype.deleteSession = function () {
  if (!this.currentSession) {
    return;
  }

  for (let win of lazy.windowManager.windows) {
    this.stopObservingWindow(win);
  }

  // reset to the top-most frame
  this.mainFrame = null;

  if (this.promptListener) {
    this.promptListener.stopListening();
    this.promptListener = null;
  }

  try {
    Services.obs.removeObserver(this, TOPIC_BROWSER_READY);
  } catch (e) {
    lazy.logger.debug(`Failed to remove observer "${TOPIC_BROWSER_READY}"`);
  }

  // Always unregister actors after all other observers
  // and listeners have been removed.
  lazy.unregisterCommandsActor();
  // MarionetteEvents actors are only disabled to avoid IPC errors if there are
  // in flight events being forwarded from the content process to the parent
  // process.
  lazy.disableEventsActor();

  if (lazy.RemoteAgent.webDriverBiDi) {
    lazy.RemoteAgent.webDriverBiDi.deleteSession();
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
 * @param {object} cmd
 * @param {string=} cmd.parameters.id
 *     Optional web element reference to take a screenshot of.
 *     If undefined, a screenshot will be taken of the document element.
 * @param {boolean=} cmd.parameters.full
 *     True to take a screenshot of the entire document element. Is only
 *     considered if <var>id</var> is not defined. Defaults to true.
 * @param {boolean=} cmd.parameters.hash
 *     True if the user requests a hash of the image data. Defaults to false.
 * @param {boolean=} cmd.parameters.scroll
 *     Scroll to element if |id| is provided. Defaults to true.
 *
 * @returns {string}
 *     If <var>hash</var> is false, PNG image encoded as Base64 encoded
 *     string.  If <var>hash</var> is true, hex digest of the SHA-256
 *     hash of the Base64 encoded string.
 *
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> is unknown.
 * @throws {NoSuchWindowError}
 *     Browsing context has been discarded.
 * @throws {StaleElementReferenceError}
 *     If element represented by reference <var>id</var> has gone stale.
 */
GeckoDriver.prototype.takeScreenshot = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  let { id, full, hash, scroll } = cmd.parameters;
  let format = hash ? lazy.capture.Format.Hash : lazy.capture.Format.Base64;

  full = typeof full == "undefined" ? true : full;
  scroll = typeof scroll == "undefined" ? true : scroll;

  let webEl = id ? lazy.WebElement.fromUUID(id).toJSON() : null;

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
GeckoDriver.prototype.getScreenOrientation = function () {
  lazy.assert.mobile();
  lazy.assert.open(this.getBrowsingContext({ top: true }));

  const win = this.getCurrentWindow();

  return win.screen.orientation.type;
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
GeckoDriver.prototype.setScreenOrientation = async function (cmd) {
  lazy.assert.mobile();
  lazy.assert.open(this.getBrowsingContext({ top: true }));

  const ors = [
    "portrait",
    "landscape",
    "portrait-primary",
    "landscape-primary",
    "portrait-secondary",
    "landscape-secondary",
  ];

  let or = String(cmd.parameters.orientation);
  lazy.assert.string(or);
  let mozOr = or.toLowerCase();
  if (!ors.includes(mozOr)) {
    throw new lazy.error.InvalidArgumentError(
      `Unknown screen orientation: ${or}`
    );
  }

  const win = this.getCurrentWindow();

  try {
    await win.screen.orientation.lock(mozOr);
  } catch (e) {
    throw new lazy.error.WebDriverError(
      `Unable to set screen orientation: ${or}`
    );
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
 * @returns {Object<string, number>}
 *     Window rect and window state.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.minimizeWindow = async function () {
  lazy.assert.desktop();
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (lazy.WindowState.from(win.windowState)) {
    case lazy.WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case lazy.WindowState.Maximized:
      await restoreWindow(win);
      break;
  }

  if (lazy.WindowState.from(win.windowState) != lazy.WindowState.Minimized) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new lazy.TimedPromise(
      resolve => {
        cb = new lazy.DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.minimize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
    await new lazy.IdlePromise(win);
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
 * @returns {Object<string, number>}
 *     Window rect.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.maximizeWindow = async function () {
  lazy.assert.desktop();
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (lazy.WindowState.from(win.windowState)) {
    case lazy.WindowState.Fullscreen:
      await exitFullscreen(win);
      break;

    case lazy.WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (lazy.WindowState.from(win.windowState) != lazy.WindowState.Maximized) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new lazy.TimedPromise(
      resolve => {
        cb = new lazy.DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.maximize();
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
    await new lazy.IdlePromise(win);
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
 * @returns {Map.<string, number>}
 *     Window rect.
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available for current application.
 */
GeckoDriver.prototype.fullscreenWindow = async function () {
  lazy.assert.desktop();
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const win = this.getCurrentWindow();
  switch (lazy.WindowState.from(win.windowState)) {
    case lazy.WindowState.Maximized:
    case lazy.WindowState.Minimized:
      await restoreWindow(win);
      break;
  }

  if (lazy.WindowState.from(win.windowState) != lazy.WindowState.Fullscreen) {
    let cb;
    // Use a timed promise to abort if no window manager is present
    await new lazy.TimedPromise(
      resolve => {
        cb = new lazy.DebounceCallback(resolve);
        win.addEventListener("sizemodechange", cb);
        win.fullScreen = true;
      },
      { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
    );
    win.removeEventListener("sizemodechange", cb);
  }
  await new lazy.IdlePromise(win);

  return this.curBrowser.rect;
};

/**
 * Dismisses a currently displayed modal dialogs, or returns no such alert if
 * no modal is displayed.
 *
 * @throws {NoSuchAlertError}
 *     If there is no current user prompt.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.dismissDialog = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  const dialogClosed = this.promptListener.dialogClosed();
  this.dialog.dismiss();
  await dialogClosed;

  const win = this.getCurrentWindow();
  await new lazy.IdlePromise(win);
};

/**
 * Accepts a currently displayed dialog modal, or returns no such alert if
 * no modal is displayed.
 *
 * @throws {NoSuchAlertError}
 *     If there is no current user prompt.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.acceptDialog = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  const dialogClosed = this.promptListener.dialogClosed();
  this.dialog.accept();
  await dialogClosed;

  const win = this.getCurrentWindow();
  await new lazy.IdlePromise(win);
};

/**
 * Returns the message shown in a currently displayed modal, or returns
 * a no such alert error if no modal is currently displayed.
 *
 * @throws {NoSuchAlertError}
 *     If there is no current user prompt.
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 */
GeckoDriver.prototype.getTextFromDialog = async function () {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();
  const text = await this.dialog.getText();
  return text;
};

/**
 * Set the user prompt's value field.
 *
 * Sends keys to the input field of a currently displayed modal, or
 * returns a no such alert error if no modal is currently displayed. If
 * a modal dialog is currently displayed but has no means for text input,
 * an element not visible error is returned.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.text
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
GeckoDriver.prototype.sendKeysToDialog = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  this._checkIfAlertIsPresent();

  let text = lazy.assert.string(cmd.parameters.text);
  let promptType = this.dialog.args.promptType;

  switch (promptType) {
    case "alert":
    case "confirm":
      throw new lazy.error.ElementNotInteractableError(
        `User prompt of type ${promptType} is not interactable`
      );
    case "prompt":
      break;
    default:
      await this.dismissDialog();
      throw new lazy.error.UnsupportedOperationError(
        `User prompt of type ${promptType} is not supported`
      );
  }
  this.dialog.text = text;
};

GeckoDriver.prototype._checkIfAlertIsPresent = function () {
  if (!this.dialog || !this.dialog.isOpen) {
    throw new lazy.error.NoSuchAlertError();
  }
};

GeckoDriver.prototype._handleUserPrompts = async function () {
  if (!this.dialog || !this.dialog.isOpen) {
    return;
  }

  const textContent = await this.dialog.getText();

  const behavior = this.currentSession.unhandledPromptBehavior;
  switch (behavior) {
    case lazy.UnhandledPromptBehavior.Accept:
      await this.acceptDialog();
      break;

    case lazy.UnhandledPromptBehavior.AcceptAndNotify:
      await this.acceptDialog();
      throw new lazy.error.UnexpectedAlertOpenError(
        `Accepted user prompt dialog: ${textContent}`
      );

    case lazy.UnhandledPromptBehavior.Dismiss:
      await this.dismissDialog();
      break;

    case lazy.UnhandledPromptBehavior.DismissAndNotify:
      await this.dismissDialog();
      throw new lazy.error.UnexpectedAlertOpenError(
        `Dismissed user prompt dialog: ${textContent}`
      );

    case lazy.UnhandledPromptBehavior.Ignore:
      throw new lazy.error.UnexpectedAlertOpenError(
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
 * @param {object} cmd
 * @param {boolean} cmd.parameters.value
 *     True if the server should accept new socket connections.
 */
GeckoDriver.prototype.acceptConnections = async function (cmd) {
  lazy.assert.boolean(cmd.parameters.value);
  await this._server.setAcceptConnections(cmd.parameters.value);
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
 * @param {object} cmd
 * @param {Array.<string>=} cmd.parameters.flags
 *     Constant name of masks to pass to |Services.startup.quit|.
 *     If empty or undefined, |nsIAppStartup.eAttemptQuit| is used.
 * @param {boolean=} cmd.parameters.safeMode
 *     Optional flag to indicate that the application has to
 *     be restarted in safe mode.
 *
 * @returns {Object<string,boolean>}
 *     Dictionary containing information that explains the shutdown reason.
 *     The value for `cause` contains the shutdown kind like "shutdown" or
 *     "restart", while `forced` will indicate if it was a normal or forced
 *     shutdown of the application. "in_app" is always set to indicate that
 *     it is a shutdown triggered from within the application.
 *
 * @throws {InvalidArgumentError}
 *     If <var>flags</var> contains unknown or incompatible flags,
 *     for example multiple Quit flags.
 */
GeckoDriver.prototype.quit = async function (cmd) {
  const { flags = [], safeMode = false } = cmd.parameters;

  lazy.assert.array(flags, `Expected "flags" to be an array`);
  lazy.assert.boolean(safeMode, `Expected "safeMode" to be a boolean`);

  if (safeMode && !flags.includes("eRestart")) {
    throw new lazy.error.InvalidArgumentError(
      `"safeMode" only works with restart flag`
    );
  }

  // Register handler to run Marionette specific shutdown code.
  Services.obs.addObserver(this, TOPIC_QUIT_APPLICATION_REQUESTED);

  let quitApplicationResponse;
  try {
    quitApplicationResponse = await lazy.quit(
      flags,
      safeMode,
      this.currentSession.capabilities.get("moz:windowless")
    );
  } catch (e) {
    if (e instanceof TypeError) {
      throw new lazy.error.InvalidArgumentError(e.message);
    }

    throw new lazy.error.UnsupportedOperationError(e.message);
  } finally {
    Services.obs.removeObserver(this, TOPIC_QUIT_APPLICATION_REQUESTED);
  }

  return quitApplicationResponse;
};

GeckoDriver.prototype.installAddon = function (cmd) {
  lazy.assert.desktop();

  let path = cmd.parameters.path;
  let temp = cmd.parameters.temporary || false;
  if (
    typeof path == "undefined" ||
    typeof path != "string" ||
    typeof temp != "boolean"
  ) {
    throw new lazy.error.InvalidArgumentError();
  }

  return lazy.Addon.install(path, temp);
};

GeckoDriver.prototype.uninstallAddon = function (cmd) {
  lazy.assert.desktop();

  let id = cmd.parameters.id;
  if (typeof id == "undefined" || typeof id != "string") {
    throw new lazy.error.InvalidArgumentError();
  }

  return lazy.Addon.uninstall(id);
};

/**
 * Retrieve the localized string for the specified entity id.
 *
 * Example:
 *     localizeEntity(["chrome://branding/locale/brand.dtd"], "brandShortName")
 *
 * @param {object} cmd
 * @param {Array.<string>} cmd.parameters.urls
 *     Array of .dtd URLs.
 * @param {string} cmd.parameters.id
 *     The ID of the entity to retrieve the localized string for.
 *
 * @returns {string}
 *     The localized string for the requested entity.
 */
GeckoDriver.prototype.localizeEntity = function (cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new lazy.error.InvalidArgumentError(
      "Value of `urls` should be of type 'Array'"
    );
  }
  if (typeof id != "string") {
    throw new lazy.error.InvalidArgumentError(
      "Value of `id` should be of type 'string'"
    );
  }

  return lazy.l10n.localizeEntity(urls, id);
};

/**
 * Retrieve the localized string for the specified property id.
 *
 * Example:
 *
 *     localizeProperty(
 *         ["chrome://global/locale/findbar.properties"], "FastFind");
 *
 * @param {object} cmd
 * @param {Array.<string>} cmd.parameters.urls
 *     Array of .properties URLs.
 * @param {string} cmd.parameters.id
 *     The ID of the property to retrieve the localized string for.
 *
 * @returns {string}
 *     The localized string for the requested property.
 */
GeckoDriver.prototype.localizeProperty = function (cmd) {
  let { urls, id } = cmd.parameters;

  if (!Array.isArray(urls)) {
    throw new lazy.error.InvalidArgumentError(
      "Value of `urls` should be of type 'Array'"
    );
  }
  if (typeof id != "string") {
    throw new lazy.error.InvalidArgumentError(
      "Value of `id` should be of type 'string'"
    );
  }

  return lazy.l10n.localizeProperty(urls, id);
};

/**
 * Initialize the reftest mode
 */
GeckoDriver.prototype.setupReftest = async function (cmd) {
  if (this._reftest) {
    throw new lazy.error.UnsupportedOperationError(
      "Called reftest:setup with a reftest session already active"
    );
  }

  let {
    urlCount = {},
    screenshot = "unexpected",
    isPrint = false,
  } = cmd.parameters;
  if (!["always", "fail", "unexpected"].includes(screenshot)) {
    throw new lazy.error.InvalidArgumentError(
      "Value of `screenshot` should be 'always', 'fail' or 'unexpected'"
    );
  }

  this._reftest = new lazy.reftest.Runner(this);
  this._reftest.setup(urlCount, screenshot, isPrint);
};

/** Run a reftest. */
GeckoDriver.prototype.runReftest = function (cmd) {
  let { test, references, expected, timeout, width, height, pageRanges } =
    cmd.parameters;

  if (!this._reftest) {
    throw new lazy.error.UnsupportedOperationError(
      "Called reftest:run before reftest:start"
    );
  }

  lazy.assert.string(test);
  lazy.assert.string(expected);
  lazy.assert.array(references);

  return this._reftest.run(
    test,
    references,
    expected,
    timeout,
    pageRanges,
    width,
    height
  );
};

/**
 * End a reftest run.
 *
 * Closes the reftest window (without changing the current window handle),
 * and removes cached canvases.
 */
GeckoDriver.prototype.teardownReftest = function () {
  if (!this._reftest) {
    throw new lazy.error.UnsupportedOperationError(
      "Called reftest:teardown before reftest:start"
    );
  }

  this._reftest.teardown();
  this._reftest = null;
};

/**
 * Print page as PDF.
 *
 * @param {object} cmd
 * @param {boolean=} cmd.parameters.background
 *     Whether or not to print background colors and images.
 *     Defaults to false, which prints without background graphics.
 * @param {number=} cmd.parameters.margin.bottom
 *     Bottom margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} cmd.parameters.margin.left
 *     Left margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} cmd.parameters.margin.right
 *     Right margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {number=} cmd.parameters.margin.top
 *     Top margin in cm. Defaults to 1cm (~0.4 inches).
 * @param {('landscape'|'portrait')=} cmd.parameters.options.orientation
 *     Paper orientation. Defaults to 'portrait'.
 * @param {Array.<string|number>=} cmd.parameters.pageRanges
 *     Paper ranges to print, e.g., ['1-5', 8, '11-13'].
 *     Defaults to the empty array, which means print all pages.
 * @param {number=} cmd.parameters.page.height
 *     Paper height in cm. Defaults to US letter height (27.94cm / 11 inches)
 * @param {number=} cmd.parameters.page.width
 *     Paper width in cm. Defaults to US letter width (21.59cm / 8.5 inches)
 * @param {number=} cmd.parameters.scale
 *     Scale of the webpage rendering. Defaults to 1.0.
 * @param {boolean=} cmd.parameters.shrinkToFit
 *     Whether or not to override page size as defined by CSS.
 *     Defaults to true, in which case the content will be scaled
 *     to fit the paper size.
 *
 * @returns {string}
 *     Base64 encoded PDF representing printed document
 *
 * @throws {NoSuchWindowError}
 *     Top-level browsing context has been discarded.
 * @throws {UnexpectedAlertOpenError}
 *     A modal dialog is open, blocking this operation.
 * @throws {UnsupportedOperationError}
 *     Not available in chrome context.
 */
GeckoDriver.prototype.print = async function (cmd) {
  lazy.assert.content(this.context);
  lazy.assert.open(this.getBrowsingContext({ top: true }));
  await this._handleUserPrompts();

  const settings = lazy.print.addDefaultSettings(cmd.parameters);
  for (const prop of ["top", "bottom", "left", "right"]) {
    lazy.assert.positiveNumber(
      settings.margin[prop],
      lazy.pprint`margin.${prop} is not a positive number`
    );
  }
  for (const prop of ["width", "height"]) {
    lazy.assert.positiveNumber(
      settings.page[prop],
      lazy.pprint`page.${prop} is not a positive number`
    );
  }
  lazy.assert.positiveNumber(
    settings.scale,
    `scale ${settings.scale} is not a positive number`
  );
  lazy.assert.that(
    s =>
      s >= lazy.print.minScaleValue &&
      settings.scale <= lazy.print.maxScaleValue,
    `scale ${settings.scale} is outside the range ${lazy.print.minScaleValue}-${lazy.print.maxScaleValue}`
  )(settings.scale);
  lazy.assert.boolean(settings.shrinkToFit);
  lazy.assert.that(
    orientation => lazy.print.defaults.orientationValue.includes(orientation),
    `orientation ${
      settings.orientation
    } doesn't match allowed values "${lazy.print.defaults.orientationValue.join(
      "/"
    )}"`
  )(settings.orientation);
  lazy.assert.boolean(settings.background);
  lazy.assert.array(settings.pageRanges);

  const browsingContext = this.curBrowser.tab.linkedBrowser.browsingContext;
  const printSettings = await lazy.print.getPrintSettings(settings);
  const binaryString = await lazy.print.printToBinaryString(
    browsingContext,
    printSettings
  );

  return btoa(binaryString);
};

GeckoDriver.prototype.addVirtualAuthenticator = function (cmd) {
  const {
    protocol,
    transport,
    hasResidentKey,
    hasUserVerification,
    isUserConsenting,
    isUserVerified,
  } = cmd.parameters;

  lazy.assert.string(
    protocol,
    "addVirtualAuthenticator: protocol must be a string"
  );
  lazy.assert.string(
    transport,
    "addVirtualAuthenticator: transport must be a string"
  );
  lazy.assert.boolean(
    hasResidentKey,
    "addVirtualAuthenticator: hasResidentKey must be a boolean"
  );
  lazy.assert.boolean(
    hasUserVerification,
    "addVirtualAuthenticator: hasUserVerification must be a boolean"
  );
  lazy.assert.boolean(
    isUserConsenting,
    "addVirtualAuthenticator: isUserConsenting must be a boolean"
  );
  lazy.assert.boolean(
    isUserVerified,
    "addVirtualAuthenticator: isUserVerified must be a boolean"
  );

  return lazy.webauthn.addVirtualAuthenticator(
    protocol,
    transport,
    hasResidentKey,
    hasUserVerification,
    isUserConsenting,
    isUserVerified
  );
};

GeckoDriver.prototype.removeVirtualAuthenticator = function (cmd) {
  const { authenticatorId } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "removeVirtualAuthenticator: authenticatorId must be a positiveInteger"
  );

  lazy.webauthn.removeVirtualAuthenticator(authenticatorId);
};

GeckoDriver.prototype.addCredential = function (cmd) {
  const {
    authenticatorId,
    credentialId,
    isResidentCredential,
    rpId,
    privateKey,
    userHandle,
    signCount,
  } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "addCredential: authenticatorId must be a positiveInteger"
  );
  lazy.assert.string(
    credentialId,
    "addCredential: credentialId must be a string"
  );
  lazy.assert.boolean(
    isResidentCredential,
    "addCredential: isResidentCredential must be a boolean"
  );
  lazy.assert.string(rpId, "addCredential: rpId must be a string");
  lazy.assert.string(privateKey, "addCredential: privateKey must be a string");
  if (userHandle) {
    lazy.assert.string(
      userHandle,
      "addCredential: userHandle must be a string if present"
    );
  }
  lazy.assert.number(signCount, "addCredential: signCount must be a number");

  lazy.webauthn.addCredential(
    authenticatorId,
    credentialId,
    isResidentCredential,
    rpId,
    privateKey,
    userHandle,
    signCount
  );
};

GeckoDriver.prototype.getCredentials = function (cmd) {
  const { authenticatorId } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "getCredentials: authenticatorId must be a positiveInteger"
  );

  return lazy.webauthn.getCredentials(authenticatorId);
};

GeckoDriver.prototype.removeCredential = function (cmd) {
  const { authenticatorId, credentialId } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "removeCredential: authenticatorId must be a positiveInteger"
  );
  lazy.assert.string(
    credentialId,
    "removeCredential: credentialId must be a string"
  );

  lazy.webauthn.removeCredential(authenticatorId, credentialId);
};

GeckoDriver.prototype.removeAllCredentials = function (cmd) {
  const { authenticatorId } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "removeAllCredentials: authenticatorId must be a positiveInteger"
  );

  lazy.webauthn.removeAllCredentials(authenticatorId);
};

GeckoDriver.prototype.setUserVerified = function (cmd) {
  const { authenticatorId, isUserVerified } = cmd.parameters;

  lazy.assert.positiveInteger(
    authenticatorId,
    "setUserVerified: authenticatorId must be a positiveInteger"
  );
  lazy.assert.boolean(
    isUserVerified,
    "setUserVerified: isUserVerified must be a boolean"
  );

  lazy.webauthn.setUserVerified(authenticatorId, isUserVerified);
};

GeckoDriver.prototype.setPermission = async function (cmd) {
  const { descriptor, state, oneRealm = false } = cmd.parameters;

  lazy.assert.boolean(oneRealm);
  lazy.assert.that(
    state => ["granted", "denied", "prompt"].includes(state),
    `state is ${state}, expected "granted", "denied", or "prompt"`
  )(state);

  lazy.permissions.set(
    descriptor,
    state,
    oneRealm,
    this.getBrowsingContext(),
    this.getBrowsingContext({ top: true })
  );
};

/**
 * Determines the Accessibility label for this element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Web element reference ID to the element for which the accessibility label
 *     will be returned.
 *
 * @returns {string}
 *     The Accessibility label for this element
 */
GeckoDriver.prototype.getComputedLabel = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();

  return this.getActor().getComputedLabel(webEl);
};

/**
 * Determines the Accessibility role for this element.
 *
 * @param {object} cmd
 * @param {string} cmd.parameters.id
 *     Web element reference ID to the element for which the accessibility role
 *     will be returned.
 *
 * @returns {string}
 *     The Accessibility role for this element
 */
GeckoDriver.prototype.getComputedRole = async function (cmd) {
  lazy.assert.open(this.getBrowsingContext());
  await this._handleUserPrompts();

  let id = lazy.assert.string(cmd.parameters.id);
  let webEl = lazy.WebElement.fromUUID(id).toJSON();
  return this.getActor().getComputedRole(webEl);
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
  "WebDriver:FindElementFromShadowRoot":
    GeckoDriver.prototype.findElementFromShadowRoot,
  "WebDriver:FindElements": GeckoDriver.prototype.findElements,
  "WebDriver:FindElementsFromShadowRoot":
    GeckoDriver.prototype.findElementsFromShadowRoot,
  "WebDriver:Forward": GeckoDriver.prototype.goForward,
  "WebDriver:FullscreenWindow": GeckoDriver.prototype.fullscreenWindow,
  "WebDriver:GetActiveElement": GeckoDriver.prototype.getActiveElement,
  "WebDriver:GetAlertText": GeckoDriver.prototype.getTextFromDialog,
  "WebDriver:GetCapabilities": GeckoDriver.prototype.getSessionCapabilities,
  "WebDriver:GetComputedLabel": GeckoDriver.prototype.getComputedLabel,
  "WebDriver:GetComputedRole": GeckoDriver.prototype.getComputedRole,
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

  // WebAuthn
  "WebAuthn:AddVirtualAuthenticator":
    GeckoDriver.prototype.addVirtualAuthenticator,
  "WebAuthn:RemoveVirtualAuthenticator":
    GeckoDriver.prototype.removeVirtualAuthenticator,
  "WebAuthn:AddCredential": GeckoDriver.prototype.addCredential,
  "WebAuthn:GetCredentials": GeckoDriver.prototype.getCredentials,
  "WebAuthn:RemoveCredential": GeckoDriver.prototype.removeCredential,
  "WebAuthn:RemoveAllCredentials": GeckoDriver.prototype.removeAllCredentials,
  "WebAuthn:SetUserVerified": GeckoDriver.prototype.setUserVerified,
};

async function exitFullscreen(win) {
  let cb;
  // Use a timed promise to abort if no window manager is present
  await new lazy.TimedPromise(
    resolve => {
      cb = new lazy.DebounceCallback(resolve);
      win.addEventListener("sizemodechange", cb);
      win.fullScreen = false;
    },
    { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
  win.removeEventListener("sizemodechange", cb);
  await new lazy.IdlePromise(win);
}

async function restoreWindow(win) {
  let cb;
  if (lazy.WindowState.from(win.windowState) == lazy.WindowState.Normal) {
    return;
  }
  // Use a timed promise to abort if no window manager is present
  await new lazy.TimedPromise(
    resolve => {
      cb = new lazy.DebounceCallback(resolve);
      win.addEventListener("sizemodechange", cb);
      win.restore();
    },
    { throws: null, timeout: TIMEOUT_NO_WINDOW_MANAGER }
  );
  win.removeEventListener("sizemodechange", cb);
  await new lazy.IdlePromise(win);
}
