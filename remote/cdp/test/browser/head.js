/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// window.RemoteAgent is a simple object set in browser.js, and importing
// RemoteAgent conflicts with that.
// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
const { RemoteAgent } = ChromeUtils.importESModule(
  "chrome://remote/content/components/RemoteAgent.sys.mjs"
);
const { RemoteAgentError } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/Error.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);
const { Stream } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/StreamRegistry.sys.mjs"
);

const TIMEOUT_MULTIPLIER = getTimeoutMultiplier();
const TIMEOUT_EVENTS = 1000 * TIMEOUT_MULTIPLIER;

function getTimeoutMultiplier() {
  if (
    AppConstants.DEBUG ||
    AppConstants.MOZ_CODE_COVERAGE ||
    AppConstants.ASAN ||
    AppConstants.TSAN
  ) {
    return 4;
  }

  return 1;
}

/*
add_task() is overriden to setup and teardown a test environment
making it easier to  write browser-chrome tests for the remote
debugger.

Before the task is run, the nsIRemoteAgent listener is started and
a CDP client is connected to it.  A new tab is also added.  These
three things are exposed to the provided task like this:

	add_task(async function testName(client, CDP, tab) {
	  // client is an instance of the CDP class
	  // CDP is ./chrome-remote-interface.js
	  // tab is a fresh tab, destroyed after the test
	});

Also target discovery is getting enabled, which means that targetCreated,
targetDestroyed, and targetInfoChanged events will be received by the client.

add_plain_task() may be used to write test tasks without the implicit
setup and teardown described above.
*/

const add_plain_task = add_task.bind(this);

this.add_task = function (taskFn, opts = {}) {
  const {
    createTab = true, // By default run each test in its own tab
  } = opts;

  const fn = async function () {
    let client, tab, target;

    try {
      const CDP = await getCDP();

      if (createTab) {
        tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
        const tabId = TabManager.getIdForBrowser(tab.linkedBrowser);

        const targets = await CDP.List();
        target = targets.find(target => target.id === tabId);
      }

      client = await CDP({ target });
      info("CDP client instantiated");

      // Bug 1605722 - Workaround to not hang when waiting for Target events
      await getDiscoveredTargets(client.Target);

      await taskFn({ client, CDP, tab });

      if (createTab) {
        // taskFn may resolve within a tick after opening a new tab.
        // We shouldn't remove the newly opened tab in the same tick.
        // Wait for the next tick here.
        await TestUtils.waitForTick();
        BrowserTestUtils.removeTab(tab);
      }
    } catch (e) {
      // Display better error message with the server side stacktrace
      // if an error happened on the server side:
      if (e.response) {
        throw RemoteAgentError.fromJSON(e.response);
      } else {
        throw e;
      }
    } finally {
      if (client) {
        await client.close();
        info("CDP client closed");
      }

      // Close any additional tabs, so that only a single tab remains open
      while (gBrowser.tabs.length > 1) {
        gBrowser.removeCurrentTab();
      }
    }
  };

  Object.defineProperty(fn, "name", { value: taskFn.name, writable: false });
  add_plain_task(fn);
};

/**
 * Create a test document in an invisible window.
 * This window will be automatically closed on test teardown.
 */
function createTestDocument() {
  const browser = Services.appShell.createWindowlessBrowser(true);
  registerCleanupFunction(() => browser.close());

  // Create a system principal content viewer to ensure there is a valid
  // empty document using system principal and avoid any wrapper issues
  // when using document's JS Objects.
  const webNavigation = browser.docShell.QueryInterface(Ci.nsIWebNavigation);
  const system = Services.scriptSecurityManager.getSystemPrincipal();
  webNavigation.createAboutBlankContentViewer(system, system);

  return webNavigation.document;
}

/**
 * Retrieve an intance of CDP object from chrome-remote-interface library
 */
async function getCDP() {
  // Instantiate a background test document in order to load the library
  // as in a web page
  const document = createTestDocument();

  const window = document.defaultView.wrappedJSObject;
  Services.scriptloader.loadSubScript(
    "chrome://mochitests/content/browser/remote/cdp/test/browser/chrome-remote-interface.js",
    window
  );

  // Implements `criRequest` to be called by chrome-remote-interface
  // library in order to do the cross-domain http request, which,
  // in a regular Web page, is impossible.
  window.criRequest = (options, callback) => {
    const { path } = options;
    const url = `http://${RemoteAgent.host}:${RemoteAgent.port}${path}`;
    const xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);

    // Prevent "XML Parsing Error: syntax error" error messages
    xhr.overrideMimeType("text/plain");

    xhr.send(null);
    xhr.onload = () => callback(null, xhr.responseText);
    xhr.onerror = e => callback(e, null);
  };

  return window.CDP;
}

async function getScrollbarSize() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const scrollbarHeight = {};
    const scrollbarWidth = {};

    content.windowUtils.getScrollbarSize(
      false,
      scrollbarWidth,
      scrollbarHeight
    );
    return {
      width: scrollbarWidth.value,
      height: scrollbarHeight.value,
    };
  });
}

function getTargets(CDP) {
  return new Promise((resolve, reject) => {
    CDP.List(null, (err, targets) => {
      if (err) {
        reject(err);
        return;
      }
      resolve(targets);
    });
  });
}

// Wait for all Target.targetCreated events. One for each tab.
async function getDiscoveredTargets(Target, options = {}) {
  const { discover = true, filter } = options;

  const targets = [];
  const unsubscribe = Target.targetCreated(target => {
    targets.push(target.targetInfo);
  });

  await Target.setDiscoverTargets({
    discover,
    filter,
  }).finally(() => unsubscribe());

  return targets;
}

async function openTab(Target, options = {}) {
  const { activate = false } = options;

  info("Create a new tab and wait for the target to be created");
  const targetCreated = Target.targetCreated();
  const newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const { targetInfo } = await targetCreated;

  is(targetInfo.type, "page");

  if (activate) {
    await Target.activateTarget({
      targetId: targetInfo.targetId,
    });
    info(`New tab with target id ${targetInfo.targetId} created and activated`);
  } else {
    info(`New tab with target id ${targetInfo.targetId} created`);
  }

  return { targetInfo, newTab };
}

async function openWindow(Target, options = {}) {
  const { activate = false } = options;

  info("Create a new window and wait for the target to be created");
  const targetCreated = Target.targetCreated();
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  const newTab = newWindow.gBrowser.selectedTab;
  const { targetInfo } = await targetCreated;
  is(targetInfo.type, "page");

  if (activate) {
    await Target.activateTarget({
      targetId: targetInfo.targetId,
    });
    info(
      `New window with target id ${targetInfo.targetId} created and activated`
    );
  } else {
    info(`New window with target id ${targetInfo.targetId} created`);
  }

  return { targetInfo, newWindow, newTab };
}

/** Creates a data URL for the given source document. */
function toDataURL(src, doctype = "html") {
  let doc, mime;
  switch (doctype) {
    case "html":
      mime = "text/html;charset=utf-8";
      doc = `<!doctype html>\n<meta charset=utf-8>\n${src}`;
      break;
    default:
      throw new Error("Unexpected doctype: " + doctype);
  }

  return `data:${mime},${encodeURIComponent(doc)}`;
}

function convertArgument(arg) {
  if (typeof arg === "bigint") {
    return { unserializableValue: `${arg.toString()}n` };
  }
  if (Object.is(arg, -0)) {
    return { unserializableValue: "-0" };
  }
  if (Object.is(arg, Infinity)) {
    return { unserializableValue: "Infinity" };
  }
  if (Object.is(arg, -Infinity)) {
    return { unserializableValue: "-Infinity" };
  }
  if (Object.is(arg, NaN)) {
    return { unserializableValue: "NaN" };
  }

  return { value: arg };
}

async function evaluate(client, contextId, pageFunction, ...args) {
  const { Runtime } = client;

  if (typeof pageFunction === "string") {
    return Runtime.evaluate({
      expression: pageFunction,
      contextId,
      returnByValue: true,
      awaitPromise: true,
    });
  } else if (typeof pageFunction === "function") {
    return Runtime.callFunctionOn({
      functionDeclaration: pageFunction.toString(),
      executionContextId: contextId,
      arguments: args.map(convertArgument),
      returnByValue: true,
      awaitPromise: true,
    });
  }
  throw new Error("pageFunction: expected 'string' or 'function'");
}

/**
 * Load a given URL in the currently selected tab
 */
async function loadURL(url, expectedURL = undefined) {
  expectedURL = expectedURL || url;

  const browser = gBrowser.selectedTab.linkedBrowser;
  const loaded = BrowserTestUtils.browserLoaded(browser, true, expectedURL);

  BrowserTestUtils.startLoadingURIString(browser, url);
  await loaded;
}

/**
 * Enable the Runtime domain
 */
async function enableRuntime(client) {
  const { Runtime } = client;

  // Enable watching for new execution context
  await Runtime.enable();
  info("Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await Runtime.executionContextCreated();
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  return context;
}

/**
 * Retrieve the value of a property on the content window.
 */
function getContentProperty(prop) {
  info(`Retrieve ${prop} on the content window`);
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [prop],
    _prop => content[_prop]
  );
}

/**
 * Retrieve all frames for the current tab as flattened list.
 *
 * @returns {Map<number, Frame>}
 *     Flattened list of frames as Map
 */
async function getFlattenedFrameTree(client) {
  const { Page } = client;

  function flatten(frames) {
    return frames.reduce((result, current) => {
      result.set(current.frame.id, current.frame);
      if (current.childFrames) {
        const frames = flatten(current.childFrames);
        result = new Map([...result, ...frames]);
      }
      return result;
    }, new Map());
  }

  const { frameTree } = await Page.getFrameTree();
  return flatten(Array(frameTree));
}

/**
 * Return a new promise, which resolves after ms have been elapsed
 */
function timeoutPromise(ms) {
  return new Promise(resolve => {
    window.setTimeout(resolve, ms);
  });
}

/** Fail a test. */
function fail(message) {
  ok(false, message);
}

/**
 * Create a stream with the specified contents.
 *
 * @param {string} contents
 *     Contents of the file.
 * @param {object} options
 * @param {string=} options.path
 *     Path of the file. Defaults to the temporary directory.
 * @param {boolean=} options.remove
 *     If true, automatically remove the file after the test. Defaults to true.
 *
 * @returns {Promise<Stream>}
 */
async function createFileStream(contents, options = {}) {
  let { path = null, remove = true } = options;

  if (!path) {
    path = await IOUtils.createUniqueFile(
      PathUtils.tempDir,
      "remote-agent.txt"
    );
  }

  await IOUtils.writeUTF8(path, contents);

  const stream = new Stream(path);
  if (remove) {
    registerCleanupFunction(() => stream.destroy());
  }

  return stream;
}

async function throwScriptError(options = {}) {
  const { inContent = true } = options;

  const addScriptErrorInternal = options => {
    const {
      flag = Ci.nsIScriptError.errorFlag,
      innerWindowId = content.windowGlobalChild.innerWindowId,
    } = options;

    const scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(
      Ci.nsIScriptError
    );
    scriptError.initWithWindowID(
      options.text,
      options.sourceName || "sourceName",
      null,
      options.lineNumber || 0,
      options.columnNumber || 0,
      flag,
      options.category || "javascript",
      innerWindowId
    );
    Services.console.logMessage(scriptError);
  };

  if (inContent) {
    SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [options],
      addScriptErrorInternal
    );
  } else {
    options.innerWindowId = window.windowGlobalChild.innerWindowId;
    addScriptErrorInternal(options);
  }
}

class RecordEvents {
  /**
   * A timeline of events chosen by calls to `addRecorder`.
   * Call `configure`` for each client event you want to record.
   * Then `await record(someTimeout)` to record a timeline that you
   * can make assertions about.
   *
   * const history = new RecordEvents(expectedNumberOfEvents);
   *
   * history.addRecorder({
   *  event: Runtime.executionContextDestroyed,
   *  eventName: "Runtime.executionContextDestroyed",
   *  messageFn: payload => {
   *    return `Received Runtime.executionContextDestroyed for id ${payload.executionContextId}`;
   *  },
   * });
   *
   *
   * @param {number} total
   *     Number of expected events. Stop recording when this number is exceeded.
   *
   */
  constructor(total) {
    this.events = [];
    this.promises = new Set();
    this.subscriptions = new Set();
    this.total = total;
  }

  /**
   * Configure an event to be recorded and logged.
   * The recording stops once we accumulate more than the expected
   * total of all configured events.
   *
   * @param {object} options
   * @param {CDPEvent} options.event
   *     https://github.com/cyrus-and/chrome-remote-interface#clientdomaineventcallback
   * @param {string} options.eventName
   *     Name to use for reporting.
   * @param {Function=} options.callback
   *     ({ eventName, payload }) => {} to be called when each event is received
   * @param {function(payload):string=} options.messageFn
   */
  addRecorder(options = {}) {
    const {
      event,
      eventName,
      messageFn = () => `Recorded ${eventName}`,
      callback,
    } = options;

    const promise = new Promise(resolve => {
      const unsubscribe = event(payload => {
        info(messageFn(payload));
        this.events.push({ eventName, payload, index: this.events.length });
        callback?.({ eventName, payload, index: this.events.length - 1 });
        if (this.events.length > this.total) {
          this.subscriptions.delete(unsubscribe);
          unsubscribe();
          resolve(this.events);
        }
      });
      this.subscriptions.add(unsubscribe);
    });

    this.promises.add(promise);
  }

  /**
   * Register a promise to await while recording the timeline. The returned
   * callback resolves the registered promise and adds `step`
   * to the timeline, along with an associated payload, if provided.
   *
   * @param {string} step
   * @returns {Function} callback
   */
  addPromise(step) {
    let callback;
    const promise = new Promise(resolve => {
      callback = value => {
        resolve();
        info(`Recorded ${step}`);
        this.events.push({
          eventName: step,
          payload: value,
          index: this.events.length,
        });
        return value;
      };
    });

    this.promises.add(promise);
    return callback;
  }

  /**
   * Record events until we hit the timeout or the expected total is exceeded.
   *
   * @param {number=} timeout
   *     Timeout in milliseconds. Defaults to 1000.
   *
   * @returns {Array<{ eventName, payload, index }>} Recorded events
   */
  async record(timeout = TIMEOUT_EVENTS) {
    await Promise.race([Promise.all(this.promises), timeoutPromise(timeout)]);
    for (const unsubscribe of this.subscriptions) {
      unsubscribe();
    }
    return this.events;
  }

  /**
   * Filter events based on predicate
   *
   * @param {Function} predicate
   *
   * @returns {Array<{ eventName, payload, index }>}
   *     The list of events matching the filter.
   */
  filter(predicate) {
    return this.events.filter(predicate);
  }

  /**
   * Find first occurrence of the given event.
   *
   * @param {string} eventName
   *
   * @returns {{ eventName, payload, index }} The event, if any.
   */
  findEvent(eventName) {
    const event = this.events.find(el => el.eventName == eventName);
    if (event) {
      return event;
    }
    return {};
  }

  /**
   * Find given events.
   *
   * @param {string} eventName
   *
   * @returns {Array<{ eventName, payload, index }>}
   *     The events, if any.
   */
  findEvents(eventName) {
    return this.events.filter(event => event.eventName == eventName);
  }

  /**
   * Find index of first occurrence of the given event.
   *
   * @param {string} eventName
   *
   * @returns {number} The event index, -1 if not found.
   */
  indexOf(eventName) {
    const event = this.events.find(el => el.eventName == eventName);
    if (event) {
      return event.index;
    }
    return -1;
  }
}
