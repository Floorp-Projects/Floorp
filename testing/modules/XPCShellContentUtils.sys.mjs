/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ExtensionUtils } from "resource://gre/modules/ExtensionUtils.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

// Windowless browsers can create documents that rely on XUL Custom Elements:
ChromeUtils.importESModule(
  "resource://gre/modules/CustomElementsListener.sys.mjs"
);

// Need to import ActorManagerParent.sys.mjs so that the actors are initialized
// before running extension XPCShell tests.
ChromeUtils.importESModule("resource://gre/modules/ActorManagerParent.sys.mjs");

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContentTask: "resource://testing-common/ContentTask.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  SpecialPowersParent: "resource://testing-common/SpecialPowersParent.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  proxyService: [
    "@mozilla.org/network/protocol-proxy-service;1",
    "nsIProtocolProxyService",
  ],
});

const { promiseDocumentLoaded, promiseEvent, promiseObserved } = ExtensionUtils;

var gRemoteContentScripts = Services.appinfo.browserTabsRemoteAutostart;
const REMOTE_CONTENT_SUBFRAMES = Services.appinfo.fissionAutostart;

function frameScript() {
  // We need to make sure that the ExtensionPolicy service has been initialized
  // as it sets up the observers that inject extension content scripts.
  Cc["@mozilla.org/addons/policy-service;1"].getService();

  Services.obs.notifyObservers(this, "tab-content-frameloader-created");

  // eslint-disable-next-line mozilla/balanced-listeners, no-undef
  addEventListener(
    "MozHeapMinimize",
    () => {
      Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");
    },
    true,
    true
  );
}

let kungFuDeathGrip = new Set();
function promiseBrowserLoaded(browser, url, redirectUrl) {
  url = url && Services.io.newURI(url);
  redirectUrl = redirectUrl && Services.io.newURI(redirectUrl);

  return new Promise(resolve => {
    const listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsISupportsWeakReference",
        "nsIWebProgressListener",
      ]),

      onStateChange(webProgress, request, stateFlags, statusCode) {
        request.QueryInterface(Ci.nsIChannel);

        let requestURI =
          request.originalURI ||
          webProgress.DOMWindow.document.documentURIObject;
        if (
          webProgress.isTopLevel &&
          (url?.equals(requestURI) || redirectUrl?.equals(requestURI)) &&
          stateFlags & Ci.nsIWebProgressListener.STATE_STOP
        ) {
          resolve();
          kungFuDeathGrip.delete(listener);
          browser.removeProgressListener(listener);
        }
      },
    };

    // addProgressListener only supports weak references, so we need to
    // use one. But we also need to make sure it stays alive until we're
    // done with it, so thunk away a strong reference to keep it alive.
    kungFuDeathGrip.add(listener);
    browser.addProgressListener(
      listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW
    );
  });
}

class ContentPage {
  constructor(
    remote = gRemoteContentScripts,
    remoteSubframes = REMOTE_CONTENT_SUBFRAMES,
    extension = null,
    privateBrowsing = false,
    userContextId = undefined
  ) {
    this.remote = remote;

    // If an extension has been passed, overwrite remote
    // with extension.remote to be sure that the ContentPage
    // will have the same remoteness of the extension.
    if (extension) {
      this.remote = extension.remote;
    }

    this.remoteSubframes = this.remote && remoteSubframes;
    this.extension = extension;
    this.privateBrowsing = privateBrowsing;
    this.userContextId = userContextId;

    this.browserReady = this._initBrowser();
  }

  async _initBrowser() {
    let chromeFlags = 0;
    if (this.remote) {
      chromeFlags |= Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW;
    }
    if (this.remoteSubframes) {
      chromeFlags |= Ci.nsIWebBrowserChrome.CHROME_FISSION_WINDOW;
    }
    if (this.privateBrowsing) {
      chromeFlags |= Ci.nsIWebBrowserChrome.CHROME_PRIVATE_WINDOW;
    }
    this.windowlessBrowser = Services.appShell.createWindowlessBrowser(
      true,
      chromeFlags
    );

    let system = Services.scriptSecurityManager.getSystemPrincipal();

    let chromeShell = this.windowlessBrowser.docShell.QueryInterface(
      Ci.nsIWebNavigation
    );

    chromeShell.createAboutBlankContentViewer(system, system);
    this.windowlessBrowser.browsingContext.useGlobalHistory = false;
    let loadURIOptions = {
      triggeringPrincipal: system,
    };
    chromeShell.loadURI(
      Services.io.newURI("chrome://extensions/content/dummy.xhtml"),
      loadURIOptions
    );

    await promiseObserved(
      "chrome-document-global-created",
      win => win.document == chromeShell.document
    );

    let chromeDoc = await promiseDocumentLoaded(chromeShell.document);

    let { SpecialPowers } = chromeDoc.ownerGlobal;
    SpecialPowers.xpcshellScope = XPCShellContentUtils.currentScope;
    SpecialPowers.setAsDefaultAssertHandler();

    let browser = chromeDoc.createXULElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("messagemanagergroup", "webext-browsers");
    browser.setAttribute("nodefaultsrc", "true");
    if (this.userContextId) {
      browser.setAttribute("usercontextid", this.userContextId);
    }

    if (this.extension?.remote) {
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", "extension");
    }

    // Ensure that the extension is loaded into the correct
    // BrowsingContextGroupID by default.
    if (this.extension) {
      browser.setAttribute(
        "initialBrowsingContextGroupId",
        this.extension.browsingContextGroupId
      );
    }

    let awaitFrameLoader = Promise.resolve();
    if (this.remote) {
      awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
      browser.setAttribute("remote", "true");

      browser.setAttribute("maychangeremoteness", "true");
      browser.addEventListener(
        "DidChangeBrowserRemoteness",
        this.didChangeBrowserRemoteness.bind(this)
      );
    }

    chromeDoc.documentElement.appendChild(browser);

    // Forcibly flush layout so that we get a pres shell soon enough, see
    // bug 1274775.
    browser.getBoundingClientRect();

    await awaitFrameLoader;

    this.browser = browser;

    this.loadFrameScript(frameScript);

    return browser;
  }

  get browsingContext() {
    return this.browser.browsingContext;
  }

  get SpecialPowers() {
    return this.browser.ownerGlobal.SpecialPowers;
  }

  loadFrameScript(func) {
    let frameScript = `data:text/javascript,(${encodeURI(func)}).call(this)`;
    this.browser.messageManager.loadFrameScript(frameScript, true, true);
  }

  addFrameScriptHelper(func) {
    let frameScript = `data:text/javascript,${encodeURI(func)}`;
    this.browser.messageManager.loadFrameScript(frameScript, false, true);
  }

  didChangeBrowserRemoteness(event) {
    // XXX: Tests can load their own additional frame scripts, so we may need to
    // track all scripts that have been loaded, and reload them here?
    this.loadFrameScript(frameScript);
  }

  async loadURL(url, redirectUrl = undefined) {
    await this.browserReady;

    this.browser.fixupAndLoadURIString(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });
    return promiseBrowserLoaded(this.browser, url, redirectUrl);
  }

  async fetch(...args) {
    return this.spawn(args, async (url, options) => {
      let resp = await this.content.fetch(url, options);
      return resp.text();
    });
  }

  spawn(params, task) {
    return this.SpecialPowers.spawn(this.browser, params, task);
  }

  // Like spawn(), but uses the legacy ContentTask infrastructure rather than
  // SpecialPowers. Exists only because the author of the SpecialPowers
  // migration did not have the time to fix all of the legacy users who relied
  // on the old semantics.
  //
  // DO NOT USE IN NEW CODE
  legacySpawn(params, task) {
    lazy.ContentTask.setTestScope(XPCShellContentUtils.currentScope);

    return lazy.ContentTask.spawn(this.browser, params, task);
  }

  async close() {
    await this.browserReady;

    let { messageManager } = this.browser;

    this.browser.removeEventListener(
      "DidChangeBrowserRemoteness",
      this.didChangeBrowserRemoteness.bind(this)
    );
    this.browser = null;

    this.windowlessBrowser.close();
    this.windowlessBrowser = null;

    await lazy.TestUtils.topicObserved(
      "message-manager-disconnect",
      subject => subject === messageManager
    );
  }
}

export var XPCShellContentUtils = {
  currentScope: null,
  fetchScopes: new Map(),

  initCommon(scope) {
    this.currentScope = scope;

    // We need to load at least one frame script into every message
    // manager to ensure that the scriptable wrapper for its global gets
    // created before we try to access it externally. If we don't, we
    // fail sanity checks on debug builds the first time we try to
    // create a wrapper, because we should never have a global without a
    // cached wrapper.
    Services.mm.loadFrameScript("data:text/javascript,//", true, true);

    scope.registerCleanupFunction(() => {
      this.currentScope = null;

      return Promise.all(
        Array.from(this.fetchScopes.values(), promise =>
          promise.then(scope => scope.close())
        )
      );
    });
  },

  init(scope) {
    // QuotaManager crashes if it doesn't have a profile.
    scope.do_get_profile();

    this.initCommon(scope);

    lazy.SpecialPowersParent.registerActor();
  },

  initMochitest(scope) {
    this.initCommon(scope);
  },

  ensureInitialized(scope) {
    if (!this.currentScope) {
      if (scope.do_get_profile) {
        this.init(scope);
      } else {
        this.initMochitest(scope);
      }
    }
  },

  /**
   * Creates a new HttpServer for testing, and begins listening on the
   * specified port. Automatically shuts down the server when the test
   * unit ends.
   *
   * @param {object} [options = {}]
   *        The options object.
   * @param {integer} [options.port = -1]
   *        The port to listen on. If omitted, listen on a random
   *        port. The latter is the preferred behavior.
   * @param {sequence<string>?} [options.hosts = null]
   *        A set of hosts to accept connections to. Support for this is
   *        implemented using a proxy filter.
   *
   * @returns {HttpServer}
   *        The HTTP server instance.
   */
  createHttpServer({ port = -1, hosts } = {}) {
    let server = new lazy.HttpServer();
    server.start(port);

    if (hosts) {
      const hostsSet = new Set();
      const serverHost = "localhost";
      const serverPort = server.identity.primaryPort;

      for (let host of hosts) {
        if (host.startsWith("[") && host.endsWith("]")) {
          // HttpServer expects IPv6 addresses in bracket notation, but the
          // proxy filter uses nsIURI.host, which does not have brackets.
          hostsSet.add(host.slice(1, -1));
        } else {
          hostsSet.add(host);
        }
        server.identity.add("http", host, 80);
      }

      const proxyFilter = {
        proxyInfo: lazy.proxyService.newProxyInfo(
          "http",
          serverHost,
          serverPort,
          "",
          "",
          0,
          4096,
          null
        ),

        applyFilter(channel, defaultProxyInfo, callback) {
          if (hostsSet.has(channel.URI.host)) {
            callback.onProxyFilterResult(this.proxyInfo);
          } else {
            callback.onProxyFilterResult(defaultProxyInfo);
          }
        },
      };

      lazy.proxyService.registerChannelFilter(proxyFilter, 0);
      this.currentScope.registerCleanupFunction(() => {
        lazy.proxyService.unregisterChannelFilter(proxyFilter);
      });
    }

    this.currentScope.registerCleanupFunction(() => {
      return new Promise(resolve => {
        server.stop(resolve);
      });
    });

    return server;
  },

  registerJSON(server, path, obj) {
    server.registerPathHandler(path, (request, response) => {
      response.setHeader("content-type", "application/json", true);
      response.write(JSON.stringify(obj));
    });
  },

  get remoteContentScripts() {
    return gRemoteContentScripts;
  },

  set remoteContentScripts(val) {
    gRemoteContentScripts = !!val;
  },

  async fetch(origin, url, options) {
    let fetchScopePromise = this.fetchScopes.get(origin);
    if (!fetchScopePromise) {
      fetchScopePromise = this.loadContentPage(origin);
      this.fetchScopes.set(origin, fetchScopePromise);
    }

    let fetchScope = await fetchScopePromise;
    return fetchScope.fetch(url, options);
  },

  /**
   * Loads a content page into a hidden docShell.
   *
   * @param {string} url
   *        The URL to load.
   * @param {object} [options = {}]
   * @param {ExtensionWrapper} [options.extension]
   *        If passed, load the URL as an extension page for the given
   *        extension.
   * @param {boolean} [options.remote]
   *        If true, load the URL in a content process. If false, load
   *        it in the parent process.
   * @param {boolean} [options.remoteSubframes]
   *        If true, load cross-origin frames in separate content processes.
   *        This is ignored if |options.remote| is false.
   * @param {string} [options.redirectUrl]
   *        An optional URL that the initial page is expected to
   *        redirect to.
   *
   * @returns {ContentPage}
   */
  loadContentPage(
    url,
    {
      extension = undefined,
      remote = undefined,
      remoteSubframes = undefined,
      redirectUrl = undefined,
      privateBrowsing = false,
      userContextId = undefined,
    } = {}
  ) {
    let contentPage = new ContentPage(
      remote,
      remoteSubframes,
      extension && extension.extension,
      privateBrowsing,
      userContextId
    );

    return contentPage.loadURL(url, redirectUrl).then(() => {
      return contentPage;
    });
  },
};
