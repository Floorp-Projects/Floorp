/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["E10SUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useSeparateFileUriProcess",
  "browser.tabs.remote.separateFileUriProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "allowLinkedWebInFileUriProcess",
  "browser.tabs.remote.allowLinkedWebInFileUriProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useSeparatePrivilegedAboutContentProcess",
  "browser.tabs.remote.separatePrivilegedContentProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "separatePrivilegedMozillaWebContentProcess",
  "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "separatedMozillaDomains",
  "browser.tabs.remote.separatedMozillaDomains",
  false,
  false,
  val => val.split(",")
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useHttpResponseProcessSelection",
  "browser.tabs.remote.useHTTPResponseProcessSelection",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useCrossOriginOpenerPolicy",
  "browser.tabs.remote.useCrossOriginOpenerPolicy",
  false
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "serializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

function debug(msg) {
  Cu.reportError(new Error("E10SUtils: " + msg));
}

function getAboutModule(aURL) {
  // Needs to match NS_GetAboutModuleName
  let moduleName = aURL.pathQueryRef.replace(/[#?].*/, "").toLowerCase();
  let contract = "@mozilla.org/network/protocol/about;1?what=" + moduleName;
  try {
    return Cc[contract].getService(Ci.nsIAboutModule);
  } catch (e) {
    // Either the about module isn't defined or it is broken. In either case
    // ignore it.
    return null;
  }
}

const NOT_REMOTE = null;

// These must match any similar ones in ContentParent.h and ProcInfo.h
const WEB_REMOTE_TYPE = "web";
const FISSION_WEB_REMOTE_TYPE_PREFIX = "webIsolated=";
const FILE_REMOTE_TYPE = "file";
const EXTENSION_REMOTE_TYPE = "extension";
const PRIVILEGEDABOUT_REMOTE_TYPE = "privilegedabout";
const PRIVILEGEDMOZILLA_REMOTE_TYPE = "privilegedmozilla";

// This must start with the WEB_REMOTE_TYPE above.
const LARGE_ALLOCATION_REMOTE_TYPE = "webLargeAllocation";
const DEFAULT_REMOTE_TYPE = WEB_REMOTE_TYPE;

function validatedWebRemoteType(
  aPreferredRemoteType,
  aTargetUri,
  aCurrentUri,
  aRemoteSubframes
) {
  // To load into the Privileged Mozilla Content Process you must be https,
  // and be an exact match or a subdomain of an allowlisted domain.
  if (
    separatePrivilegedMozillaWebContentProcess &&
    aTargetUri.asciiHost &&
    aTargetUri.scheme == "https" &&
    separatedMozillaDomains.some(function(val) {
      return (
        aTargetUri.asciiHost == val || aTargetUri.asciiHost.endsWith("." + val)
      );
    })
  ) {
    return PRIVILEGEDMOZILLA_REMOTE_TYPE;
  }

  // If the domain is whitelisted to allow it to use file:// URIs, then we have
  // to run it in a file content process, in case it uses file:// sub-resources.
  const sm = Services.scriptSecurityManager;
  if (sm.inFileURIAllowlist(aTargetUri)) {
    return FILE_REMOTE_TYPE;
  }

  // If we're within a fission window, extract site information from the URI in
  // question, and use it to generate an isolated origin.
  if (aRemoteSubframes) {
    let targetPrincipal = sm.createContentPrincipal(aTargetUri, {});
    return FISSION_WEB_REMOTE_TYPE_PREFIX + targetPrincipal.siteOrigin;
  }

  if (!aPreferredRemoteType) {
    return WEB_REMOTE_TYPE;
  }

  if (aPreferredRemoteType.startsWith(WEB_REMOTE_TYPE)) {
    return aPreferredRemoteType;
  }

  if (
    allowLinkedWebInFileUriProcess &&
    aPreferredRemoteType == FILE_REMOTE_TYPE
  ) {
    // If aCurrentUri is passed then we should only allow FILE_REMOTE_TYPE
    // when it is same origin as target.
    if (aCurrentUri) {
      try {
        // checkSameOriginURI throws when not same origin.
        // todo: if you intend to update CheckSameOriginURI to log the error to the
        // console you also need to update the 'aFromPrivateWindow' argument.
        sm.checkSameOriginURI(aCurrentUri, aTargetUri, false, false);
        return FILE_REMOTE_TYPE;
      } catch (e) {
        return WEB_REMOTE_TYPE;
      }
    }

    return FILE_REMOTE_TYPE;
  }

  return WEB_REMOTE_TYPE;
}

var E10SUtils = {
  DEFAULT_REMOTE_TYPE,
  NOT_REMOTE,
  WEB_REMOTE_TYPE,
  FILE_REMOTE_TYPE,
  EXTENSION_REMOTE_TYPE,
  PRIVILEGEDABOUT_REMOTE_TYPE,
  PRIVILEGEDMOZILLA_REMOTE_TYPE,
  LARGE_ALLOCATION_REMOTE_TYPE,

  useHttpResponseProcessSelection() {
    return useHttpResponseProcessSelection;
  },
  useCrossOriginOpenerPolicy() {
    return useCrossOriginOpenerPolicy;
  },

  /**
   * Serialize csp data.
   *
   * @param {nsIContentSecurity} csp. The csp to serialize.
   * @return {String} The base64 encoded csp data.
   */
  serializeCSP(csp) {
    let serializedCSP = null;

    try {
      if (csp) {
        serializedCSP = serializationHelper.serializeToString(csp);
      }
    } catch (e) {
      debug(`Failed to serialize csp '${csp}' ${e}`);
    }
    return serializedCSP;
  },

  /**
   * Deserialize a base64 encoded csp (serialized with
   * Utils::serializeCSP).
   *
   * @param {String} csp_b64 A base64 encoded serialized csp.
   * @return {nsIContentSecurityPolicy} A deserialized csp.
   */
  deserializeCSP(csp_b64) {
    if (!csp_b64) {
      return null;
    }

    try {
      let csp = serializationHelper.deserializeObject(csp_b64);
      csp.QueryInterface(Ci.nsIContentSecurityPolicy);
      return csp;
    } catch (e) {
      debug(`Failed to deserialize csp_b64 '${csp_b64}' ${e}`);
    }
    return null;
  },

  canLoadURIInRemoteType(
    aURL,
    aRemoteSubframes,
    aRemoteType = DEFAULT_REMOTE_TYPE,
    aPreferredRemoteType = undefined
  ) {
    // We need a strict equality here because the value of `NOT_REMOTE` is
    // `null`, and there is a possibility that `undefined` is passed as an
    // argument, which might result a load in the parent process.
    if (aPreferredRemoteType === undefined) {
      aPreferredRemoteType =
        aRemoteType === NOT_REMOTE ? NOT_REMOTE : DEFAULT_REMOTE_TYPE;
    }

    return (
      aRemoteType ==
      this.getRemoteTypeForURI(
        aURL,
        true,
        aRemoteSubframes,
        aPreferredRemoteType
      )
    );
  },

  getRemoteTypeForURI(
    aURL,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentUri
  ) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    // loadURI in browser.js treats null as about:blank
    if (!aURL) {
      aURL = "about:blank";
    }

    let uri;
    try {
      uri = Services.uriFixup.createFixupURI(
        aURL,
        Ci.nsIURIFixup.FIXUP_FLAG_NONE
      );
    } catch (e) {
      // If we have an invalid URI, it's still possible that it might get
      // fixed-up into a valid URI later on. However, we don't want to return
      // aPreferredRemoteType here, in case the URI gets fixed-up into
      // something that wouldn't normally run in that process.
      return DEFAULT_REMOTE_TYPE;
    }

    return this.getRemoteTypeForURIObject(
      uri,
      aMultiProcess,
      aRemoteSubframes,
      aPreferredRemoteType,
      aCurrentUri
    );
  },

  getRemoteTypeForURIObject(
    aURI,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentUri
  ) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    switch (aURI.scheme) {
      case "javascript":
        // javascript URIs can load in any, they apply to the current document.
        return aPreferredRemoteType;

      case "data":
      case "blob":
        // We need data: and blob: URIs to load in any remote process, because
        // they need to be able to load in whatever is the current process
        // unless it is non-remote. In that case we don't want to load them in
        // the parent process, so we load them in the default remote process,
        // which is sandboxed and limits any risk.
        return aPreferredRemoteType == NOT_REMOTE
          ? DEFAULT_REMOTE_TYPE
          : aPreferredRemoteType;

      case "file":
        return useSeparateFileUriProcess
          ? FILE_REMOTE_TYPE
          : DEFAULT_REMOTE_TYPE;

      case "about":
        let module = getAboutModule(aURI);
        // If the module doesn't exist then an error page will be loading, that
        // should be ok to load in any process
        if (!module) {
          return aPreferredRemoteType;
        }

        let flags = module.getURIFlags(aURI);
        if (flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_EXTENSION_PROCESS) {
          return WebExtensionPolicy.useRemoteWebExtensions
            ? EXTENSION_REMOTE_TYPE
            : NOT_REMOTE;
        }

        if (flags & Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD) {
          if (
            flags & Ci.nsIAboutModule.URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS &&
            (useSeparatePrivilegedAboutContentProcess ||
              aURI.filePath == "logins")
          ) {
            return PRIVILEGEDABOUT_REMOTE_TYPE;
          }
          return DEFAULT_REMOTE_TYPE;
        }

        // If the about page can load in parent or child, it should be safe to
        // load in any remote type.
        if (flags & Ci.nsIAboutModule.URI_CAN_LOAD_IN_CHILD) {
          return aPreferredRemoteType;
        }

        return NOT_REMOTE;

      case "chrome":
        let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
          Ci.nsIXULChromeRegistry
        );
        if (chromeReg.mustLoadURLRemotely(aURI)) {
          return DEFAULT_REMOTE_TYPE;
        }

        if (
          chromeReg.canLoadURLRemotely(aURI) &&
          aPreferredRemoteType != NOT_REMOTE
        ) {
          return DEFAULT_REMOTE_TYPE;
        }

        return NOT_REMOTE;

      case "moz-extension":
        return WebExtensionPolicy.useRemoteWebExtensions
          ? EXTENSION_REMOTE_TYPE
          : NOT_REMOTE;

      default:
        // WebExtensions may set up protocol handlers for protocol names
        // beginning with ext+.  These may redirect to http(s) pages or to
        // moz-extension pages.  We can't actually tell here where one of
        // these pages will end up loading but Talos tests use protocol
        // handlers that redirect to extension pages that rely on this
        // behavior so a pageloader frame script is injected correctly.
        // Protocols that redirect to http(s) will just flip back to a
        // regular content process after the redirect.
        if (aURI.scheme.startsWith("ext+")) {
          return WebExtensionPolicy.useRemoteWebExtensions
            ? EXTENSION_REMOTE_TYPE
            : NOT_REMOTE;
        }

        // For any other nested URIs, we use the innerURI to determine the
        // remote type. In theory we should use the innermost URI, but some URIs
        // have fake inner URIs (e.g. about URIs with inner moz-safe-about) and
        // if such URIs are wrapped in other nested schemes like view-source:,
        // we don't want to "skip" past "about:" by going straight to the
        // innermost URI. Any URIs like this will need to be handled in the
        // cases above, so we don't still end up using the fake inner URI here.
        if (aURI instanceof Ci.nsINestedURI) {
          let innerURI = aURI.QueryInterface(Ci.nsINestedURI).innerURI;
          return this.getRemoteTypeForURIObject(
            innerURI,
            aMultiProcess,
            aRemoteSubframes,
            aPreferredRemoteType,
            aCurrentUri
          );
        }

        return validatedWebRemoteType(
          aPreferredRemoteType,
          aURI,
          aCurrentUri,
          aRemoteSubframes
        );
    }
  },

  getRemoteTypeForPrincipal(
    aPrincipal,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentPrincipal
  ) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    // We can't pick a process based on a system principal or expanded
    // principal. In fact, we should never end up with one here!
    if (aPrincipal.isSystemPrincipal || aPrincipal.isExpandedPrincipal) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    // Null principals can be loaded in any remote process.
    if (aPrincipal.isNullPrincipal) {
      return aPreferredRemoteType == NOT_REMOTE
        ? DEFAULT_REMOTE_TYPE
        : aPreferredRemoteType;
    }

    // We might care about the currently loaded URI. Pull it out of our current
    // principal. We never care about the current URI when working with a
    // non-content principal.
    let currentURI =
      aCurrentPrincipal && aCurrentPrincipal.isContentPrincipal
        ? aCurrentPrincipal.URI
        : null;
    return E10SUtils.getRemoteTypeForURIObject(
      aPrincipal.URI,
      aMultiProcess,
      aRemoteSubframes,
      aPreferredRemoteType,
      currentURI
    );
  },

  makeInputStream(data) {
    if (typeof data == "string") {
      let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
        Ci.nsISupportsCString
      );
      stream.data = data;
      return stream; // XPConnect will QI this to nsIInputStream for us.
    }

    let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
      Ci.nsISupportsCString
    );
    stream.data = data.content;

    if (data.headers) {
      let mimeStream = Cc[
        "@mozilla.org/network/mime-input-stream;1"
      ].createInstance(Ci.nsIMIMEInputStream);

      mimeStream.setData(stream);
      for (let [name, value] of data.headers) {
        mimeStream.addHeader(name, value);
      }
      return mimeStream;
    }

    return stream; // XPConnect will QI this to nsIInputStream for us.
  },

  /**
   * Serialize principal data.
   *
   * @param {nsIPrincipal} principal The principal to serialize.
   * @return {String} The base64 encoded principal data.
   */
  serializePrincipal(principal) {
    let serializedPrincipal = null;

    try {
      if (principal) {
        serializedPrincipal = btoa(
          Services.scriptSecurityManager.principalToJSON(principal)
        );
      }
    } catch (e) {
      debug(`Failed to serialize principal '${principal}' ${e}`);
    }

    return serializedPrincipal;
  },

  /**
   * Deserialize a base64 encoded principal (serialized with
   * serializePrincipal).
   *
   * @param {String} principal_b64 A base64 encoded serialized principal.
   * @return {nsIPrincipal} A deserialized principal.
   */
  deserializePrincipal(principal_b64, fallbackPrincipalCallback = null) {
    if (!principal_b64) {
      if (!fallbackPrincipalCallback) {
        debug(
          "No principal passed to deserializePrincipal and no fallbackPrincipalCallback"
        );
        return null;
      }

      return fallbackPrincipalCallback();
    }

    try {
      let principal;
      let tmpa = atob(principal_b64);
      // Both the legacy and new JSON representation of principals are stored as base64
      // The new kind are the only ones that will start with "{" when decoded.
      // We check here for the new JSON serialized, if it doesn't start with that continue using nsISerializable.
      // JSONToPrincipal accepts a *non* base64 encoded string and returns a principal or a null.
      if (tmpa.startsWith("{")) {
        principal = Services.scriptSecurityManager.JSONToPrincipal(tmpa);
      } else {
        principal = serializationHelper.deserializeObject(principal_b64);
      }
      principal.QueryInterface(Ci.nsIPrincipal);
      return principal;
    } catch (e) {
      debug(`Failed to deserialize principal_b64 '${principal_b64}' ${e}`);
    }
    if (!fallbackPrincipalCallback) {
      debug(
        "No principal passed to deserializePrincipal and no fallbackPrincipalCallback"
      );
      return null;
    }
    return fallbackPrincipalCallback();
  },

  shouldLoadURIInBrowser(
    browser,
    uri,
    multiProcess = true,
    remoteSubframes = false,
    flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE
  ) {
    let currentRemoteType = browser.remoteType;
    let requiredRemoteType;
    let uriObject;
    try {
      let fixupFlags = Ci.nsIURIFixup.FIXUP_FLAG_NONE;
      if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
        fixupFlags |= Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
      }
      if (flags & Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
        fixupFlags |= Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS;
      }
      uriObject = Services.uriFixup.createFixupURI(uri, fixupFlags);
      // Note that I had thought that we could set uri = uriObject.spec here, to
      // save on fixup later on, but that changes behavior and breaks tests.
      requiredRemoteType = this.getRemoteTypeForURIObject(
        uriObject,
        multiProcess,
        remoteSubframes,
        currentRemoteType,
        browser.currentURI
      );
    } catch (e) {
      // createFixupURI throws if it can't create a URI. If that's the case then
      // we still need to pass down the uri because docshell handles this case.
      requiredRemoteType = multiProcess ? DEFAULT_REMOTE_TYPE : NOT_REMOTE;
    }

    let mustChangeProcess = requiredRemoteType != currentRemoteType;
    let newFrameloader = false;
    if (
      browser.getAttribute("preloadedState") === "consumed" &&
      uri != "about:newtab"
    ) {
      // Leaving about:newtab from a used to be preloaded browser should run the process
      // selecting algorithm again.
      mustChangeProcess = true;
      newFrameloader = true;
    }

    return {
      uriObject,
      requiredRemoteType,
      mustChangeProcess,
      newFrameloader,
    };
  },

  shouldLoadURIInThisProcess(aURI, aRemoteSubframes) {
    let remoteType = Services.appinfo.remoteType;
    return (
      remoteType ==
      this.getRemoteTypeForURIObject(
        aURI,
        /* remote */ true,
        aRemoteSubframes,
        remoteType
      )
    );
  },

  shouldLoadURI(aDocShell, aURI, aHasPostData) {
    let remoteSubframes = aDocShell.useRemoteSubframes;

    // Inner frames should always load in the current process
    // XXX(nika): Handle shouldLoadURI-triggered process switches for remote
    // subframes! (bug 1548942)
    if (aDocShell.sameTypeParent) {
      return true;
    }

    let webNav = aDocShell.QueryInterface(Ci.nsIWebNavigation);
    let sessionHistory = webNav.sessionHistory;
    if (
      !aHasPostData &&
      Services.appinfo.remoteType == WEB_REMOTE_TYPE &&
      sessionHistory.count == 1 &&
      webNav.currentURI.spec == "about:newtab"
    ) {
      // This is possibly a preloaded browser and we're about to navigate away for
      // the first time. On the child side there is no way to tell for sure if that
      // is the case, so let's redirect this request to the parent to decide if a new
      // process is needed. But we don't currently properly handle POST data in
      // redirects (bug 1457520), so if there is POST data, don't return false here.
      return false;
    }

    // If we are performing HTTP response process selection, and are loading an
    // HTTP URI, we can start the load in the current process, and then perform
    // the switch later-on using the RedirectProcessChooser mechanism.
    //
    // We should never be sending a POST request from the parent process to a
    // http(s) uri, so make sure we switch if we're currently in that process.
    if (
      useHttpResponseProcessSelection &&
      (aURI.scheme == "http" || aURI.scheme == "https") &&
      Services.appinfo.remoteType != NOT_REMOTE
    ) {
      return true;
    }

    // If we are in a Large-Allocation process, and it wouldn't be content visible
    // to change processes, we want to load into a new process so that we can throw
    // this one out. We don't want to move into a new process if we have post data,
    // because we would accidentally throw out that data.
    if (
      !aHasPostData &&
      Services.appinfo.remoteType == LARGE_ALLOCATION_REMOTE_TYPE &&
      !aDocShell.awaitingLargeAlloc &&
      aDocShell.isOnlyToplevelInTabGroup
    ) {
      return false;
    }

    // Allow history load if loaded in this process before.
    let requestedIndex = sessionHistory.legacySHistory.requestedIndex;
    if (requestedIndex >= 0) {
      if (
        sessionHistory.legacySHistory.getEntryAtIndex(requestedIndex)
          .loadedInThisProcess
      ) {
        return true;
      }

      // If not originally loaded in this process allow it if the URI would
      // normally be allowed to load in this process by default.
      let remoteType = Services.appinfo.remoteType;
      return (
        remoteType ==
        this.getRemoteTypeForURIObject(
          aURI,
          true,
          remoteSubframes,
          remoteType,
          webNav.currentURI
        )
      );
    }

    // If the URI can be loaded in the current process then continue
    return this.shouldLoadURIInThisProcess(aURI, remoteSubframes);
  },

  redirectLoad(
    aDocShell,
    aURI,
    aReferrerInfo,
    aTriggeringPrincipal,
    aFreshProcess,
    aFlags,
    aCsp
  ) {
    // Retarget the load to the correct process
    let messageManager = aDocShell.messageManager;
    let sessionHistory = aDocShell.QueryInterface(Ci.nsIWebNavigation)
      .sessionHistory;

    messageManager.sendAsyncMessage("Browser:LoadURI", {
      loadOptions: {
        uri: aURI.spec,
        flags: aFlags || Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
        referrerInfo: this.serializeReferrerInfo(aReferrerInfo),
        triggeringPrincipal: this.serializePrincipal(
          aTriggeringPrincipal ||
            Services.scriptSecurityManager.createNullPrincipal({})
        ),
        csp: aCsp ? this.serializeCSP(aCsp) : null,
        reloadInFreshProcess: !!aFreshProcess,
      },
      historyIndex: sessionHistory.legacySHistory.requestedIndex,
    });
    return false;
  },

  wrapHandlingUserInput(aWindow, aIsHandling, aCallback) {
    var handlingUserInput;
    try {
      handlingUserInput = aWindow.windowUtils.setHandlingUserInput(aIsHandling);
      aCallback();
    } finally {
      handlingUserInput.destruct();
    }
  },

  /**
   * Serialize referrerInfo.
   *
   * @param {nsIReferrerInfo} The referrerInfo to serialize.
   * @return {String} The base64 encoded referrerInfo.
   */
  serializeReferrerInfo(referrerInfo) {
    let serialized = null;
    if (referrerInfo) {
      try {
        serialized = serializationHelper.serializeToString(referrerInfo);
      } catch (e) {
        debug(`Failed to serialize referrerInfo '${referrerInfo}' ${e}`);
      }
    }
    return serialized;
  },
  /**
   * Deserialize a base64 encoded referrerInfo
   *
   * @param {String} referrerInfo_b64 A base64 encoded serialized referrerInfo.
   * @return {nsIReferrerInfo} A deserialized referrerInfo.
   */
  deserializeReferrerInfo(referrerInfo_b64) {
    let deserialized = null;
    if (referrerInfo_b64) {
      try {
        deserialized = serializationHelper.deserializeObject(referrerInfo_b64);
        deserialized.QueryInterface(Ci.nsIReferrerInfo);
      } catch (e) {
        debug(
          `Failed to deserialize referrerInfo_b64 '${referrerInfo_b64}' ${e}`
        );
      }
    }
    return deserialized;
  },

  /**
   * Returns the pids for a remote browser and its remote subframes.
   */
  getBrowserPids(aBrowser, aRemoteSubframes) {
    if (!aBrowser.isRemoteBrowser || !aBrowser.frameLoader) {
      return [];
    }
    let tabPid = aBrowser.frameLoader.remoteTab.osPid;
    let pids = new Set();
    if (aRemoteSubframes) {
      let stack = [aBrowser.browsingContext];
      while (stack.length) {
        let bc = stack.pop();
        stack.push(...bc.getChildren());
        if (bc.currentWindowGlobal) {
          let pid = bc.currentWindowGlobal.osPid;
          if (pid != tabPid) {
            pids.add(pid);
          }
        }
      }
    }
    return [tabPid, ...pids];
  },

  /**
   * The suffix after a `=` in a remoteType is dynamic, and used to control the
   * process pool to use. The C++ version of this method is mozilla::dom::RemoteTypePrefix().
   */
  remoteTypePrefix(aRemoteType) {
    return aRemoteType.split("=")[0];
  },

  /**
   * If Fission is enabled, the remote type for a standard content process will
   * start with webIsolated=.
   */
  isWebRemoteType(aBrowser) {
    if (aBrowser.ownerGlobal.docShell.nsILoadContext.useRemoteSubframes) {
      return aBrowser.remoteType.startsWith(FISSION_WEB_REMOTE_TYPE_PREFIX);
    }
    return aBrowser.remoteType == WEB_REMOTE_TYPE;
  },
};

XPCOMUtils.defineLazyGetter(
  E10SUtils,
  "SERIALIZED_SYSTEMPRINCIPAL",
  function() {
    return E10SUtils.serializePrincipal(
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  }
);
