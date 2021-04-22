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
  "useSeparateDataUriProcess",
  "browser.tabs.remote.dataUriInDefaultWebProcess",
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
  "",
  false,
  val => val.split(",")
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useCrossOriginOpenerPolicy",
  "browser.tabs.remote.useCrossOriginOpenerPolicy",
  false
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "useOriginAttributesInRemoteType",
  "browser.tabs.remote.useOriginAttributesInRemoteType",
  false
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "serializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "extProtService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);

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
const FISSION_WEB_REMOTE_TYPE = "webIsolated";
const WEB_REMOTE_COOP_COEP_TYPE_PREFIX = "webCOOP+COEP=";
const FILE_REMOTE_TYPE = "file";
const EXTENSION_REMOTE_TYPE = "extension";
const PRIVILEGEDABOUT_REMOTE_TYPE = "privilegedabout";
const PRIVILEGEDMOZILLA_REMOTE_TYPE = "privilegedmozilla";

// This must start with the WEB_REMOTE_TYPE above.
const LARGE_ALLOCATION_REMOTE_TYPE = "webLargeAllocation";
const DEFAULT_REMOTE_TYPE = WEB_REMOTE_TYPE;

// This list is duplicated between Navigator.cpp and here because navigator
// is not accessible in this context. Please update both if the list changes.
const kSafeSchemes = [
  "bitcoin",
  "geo",
  "im",
  "irc",
  "ircs",
  "magnet",
  "mailto",
  "matrix",
  "mms",
  "news",
  "nntp",
  "openpgp4fpr",
  "sip",
  "sms",
  "smsto",
  "ssh",
  "tel",
  "urn",
  "webcal",
  "wtai",
  "xmpp",
];

// Note that even if the scheme fits the criteria for a web-handled scheme
// (ie it is compatible with the checks registerProtocolHandler uses), it may
// not be web-handled - it could still be handled via the OS by another app.
function hasPotentiallyWebHandledScheme({ scheme }) {
  // Note that `scheme` comes from a URI object so is already lowercase.
  if (kSafeSchemes.includes(scheme)) {
    return true;
  }
  if (!scheme.startsWith("web+") || scheme.length < 5) {
    return false;
  }
  // Check the rest of the scheme only consists of ascii a-z chars
  return /^[a-z]+$/.test(scheme.substr("web+".length));
}

function validatedWebRemoteType(
  aPreferredRemoteType,
  aTargetUri,
  aCurrentUri,
  aResultPrincipal,
  aRemoteSubframes,
  aIsWorker = false,
  aOriginAttributes = {}
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

  // If we're in the parent and we were passed a web-handled scheme,
  // transform it now to avoid trying to load it in the wrong process.
  if (aRemoteSubframes && hasPotentiallyWebHandledScheme(aTargetUri)) {
    // We shouldn't even get to this for a worker, throw an unexpected error
    // if we do.
    if (aIsWorker) {
      throw Components.Exception(
        "Unexpected remote worker with a web handled scheme",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    if (
      Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT &&
      Services.appinfo.remoteType.startsWith(FISSION_WEB_REMOTE_TYPE + "=")
    ) {
      // If we're in a child process, assume we're OK to load this non-web
      // URL for now. We'll either load it externally or re-evaluate once
      // we know the "real" URL to which we'll redirect.
      return Services.appinfo.remoteType;
    }

    // This doesn't work (throws) in the child - see
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1589082
    // Even if it did, it'd cause sync IPC
    // ( https://bugzilla.mozilla.org/show_bug.cgi?id=1589085 ), and this code
    // can get called several times per page load so that seems like something
    // we'd want to avoid.
    let handlerInfo = extProtService.getProtocolHandlerInfo(aTargetUri.scheme);
    try {
      if (!handlerInfo.alwaysAskBeforeHandling) {
        let app = handlerInfo.preferredApplicationHandler;
        app.QueryInterface(Ci.nsIWebHandlerApp);
        // If we get here, the default handler is a web app.
        // Target to the origin of that web app:
        let uriStr = app.uriTemplate.replace(/%s/, aTargetUri.spec);
        aTargetUri = Services.io.newURI(uriStr);
      }
    } catch (ex) {
      // It's not strange for this to throw, we just ignore it and fall through.
    }
  }

  // If the domain is whitelisted to allow it to use file:// URIs, then we have
  // to run it in a file content process, in case it uses file:// sub-resources.
  const sm = Services.scriptSecurityManager;
  if (!aIsWorker && sm.inFileURIAllowlist(aTargetUri)) {
    return FILE_REMOTE_TYPE;
  }

  // If we're within a fission window, extract site information from the URI in
  // question, and use it to generate an isolated origin.
  if (aRemoteSubframes) {
    let originAttributes = {};
    if (useOriginAttributesInRemoteType) {
      // Only use specific properties of OriginAttributes in our remoteType
      let {
        userContextId,
        privateBrowsingId,
        geckoViewSessionContextId,
      } = aOriginAttributes;
      originAttributes = {
        userContextId,
        privateBrowsingId,
        geckoViewSessionContextId,
      };
    }

    // Get a principal to use for isolation.
    let targetPrincipal;
    if (aResultPrincipal) {
      targetPrincipal = sm.principalWithOA(aResultPrincipal, originAttributes);
    } else {
      targetPrincipal = sm.createContentPrincipal(aTargetUri, originAttributes);
    }

    // If this is a special webCOOP+COEP= remote type that matches the
    // principal's siteOrigin, we don't want to override it with webIsolated=
    // as it's already isolated.
    if (
      aPreferredRemoteType &&
      aPreferredRemoteType.startsWith(
        `${WEB_REMOTE_COOP_COEP_TYPE_PREFIX}${targetPrincipal.siteOrigin}`
      )
    ) {
      return aPreferredRemoteType;
    }

    return `${FISSION_WEB_REMOTE_TYPE}=${targetPrincipal.siteOrigin}`;
  }

  if (!aPreferredRemoteType) {
    return WEB_REMOTE_TYPE;
  }

  if (aPreferredRemoteType.startsWith(WEB_REMOTE_TYPE)) {
    return aPreferredRemoteType;
  }

  return WEB_REMOTE_TYPE;
}

// remoteTypes allowed to host system-principal remote workers.
const SYSTEM_WORKERS_REMOTE_TYPES_ALLOWED = [
  NOT_REMOTE,
  PRIVILEGEDABOUT_REMOTE_TYPE,
];

var E10SUtils = {
  DEFAULT_REMOTE_TYPE,
  NOT_REMOTE,
  WEB_REMOTE_TYPE,
  WEB_REMOTE_COOP_COEP_TYPE_PREFIX,
  FILE_REMOTE_TYPE,
  EXTENSION_REMOTE_TYPE,
  PRIVILEGEDABOUT_REMOTE_TYPE,
  PRIVILEGEDMOZILLA_REMOTE_TYPE,
  LARGE_ALLOCATION_REMOTE_TYPE,
  FISSION_WEB_REMOTE_TYPE,

  useCrossOriginOpenerPolicy() {
    return useCrossOriginOpenerPolicy;
  },

  _log: null,
  _uriStr: function uriStr(aUri) {
    return aUri ? aUri.spec : "undefined";
  },

  log: function log() {
    if (!this._log) {
      this._log = console.createInstance({
        prefix: "ProcessSwitch",
        maxLogLevel: "Error", // Change to "Debug" the process switching code
      });

      this._log.debug("Setup logger");
    }

    return this._log;
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
      this.log().error(`Failed to serialize csp '${csp}' ${e}`);
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
      this.log().error(`Failed to deserialize csp_b64 '${csp_b64}' ${e}`);
    }
    return null;
  },

  canLoadURIInRemoteType(
    aURL,
    aRemoteSubframes,
    aRemoteType = DEFAULT_REMOTE_TYPE,
    aOriginAttributes = {}
  ) {
    // aRemoteType cannot be undefined, as that would cause it to default to
    // `DEFAULT_REMOTE_TYPE`. This means any falsy remote types are
    // intentionally `NOT_REMOTE`.

    return (
      aRemoteType ==
      this.getRemoteTypeForURI(
        aURL,
        true,
        aRemoteSubframes,
        aRemoteType,
        null,
        aOriginAttributes
      )
    );
  },

  getRemoteTypeForURI(
    aURL,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentUri,
    aOriginAttributes = {}
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
      uri = Services.uriFixup.getFixupURIInfo(aURL).preferredURI;
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
      aCurrentUri,
      null, //aResultPrincipal
      false, //aIsSubframe
      false, // aIsWorker
      aOriginAttributes
    );
  },

  getRemoteTypeForURIObject(
    aURI,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentUri = null,
    aResultPrincipal = null,
    aIsSubframe = false,
    aIsWorker = false,
    aOriginAttributes = {}
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
              aURI.filePath == "logins" ||
              // Force about:welcome and about:home into the privileged content process to
              // workaround code coverage test failures which result from the
              // workaround in bug 161269. Once that bug is fixed for real,
              // the about:welcome and about:home case below can be removed.
              aURI.filePath == "welcome" ||
              aURI.filePath == "home")
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
        if (WebExtensionPolicy.useRemoteWebExtensions) {
          // Extension iframes should load in the same process
          // as their outer frame, top-level ones should load
          // in the extension process.
          return aIsSubframe ? aPreferredRemoteType : EXTENSION_REMOTE_TYPE;
        }

        return NOT_REMOTE;

      case "imap":
      case "mailbox":
      case "news":
      case "nntp":
      case "snews":
        // Protocols used by Thunderbird to display email messages.
        return NOT_REMOTE;

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
          // We shouldn't even get to this for a worker, throw an unexpected error
          // if we do.
          if (aIsWorker) {
            throw Components.Exception(
              "Unexpected remote worker with extension handled scheme",
              Cr.NS_ERROR_UNEXPECTED
            );
          }

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
          // We shouldn't even get to this for a worker, throw an unexpected error
          // if we do.
          if (aIsWorker) {
            throw Components.Exception(
              "Unexpected worker with a NestedURI",
              Cr.NS_ERROR_UNEXPECTED
            );
          }

          let innerURI = aURI.QueryInterface(Ci.nsINestedURI).innerURI;
          return this.getRemoteTypeForURIObject(
            innerURI,
            aMultiProcess,
            aRemoteSubframes,
            aPreferredRemoteType,
            aCurrentUri,
            aResultPrincipal,
            false, // aIsSubframe
            false, // aIsWorker
            aOriginAttributes
          );
        }

        var log = this.log();
        log.debug("validatedWebRemoteType()");
        log.debug(`  aPreferredRemoteType: ${aPreferredRemoteType}`);
        log.debug(`  aTargetUri: ${this._uriStr(aURI)}`);
        log.debug(`  aCurrentUri: ${this._uriStr(aCurrentUri)}`);
        var remoteType = validatedWebRemoteType(
          aPreferredRemoteType,
          aURI,
          aCurrentUri,
          aResultPrincipal,
          aRemoteSubframes,
          aIsWorker,
          aOriginAttributes
        );
        log.debug(`  validatedWebRemoteType() returning: ${remoteType}`);
        return remoteType;
    }
  },

  getRemoteTypeForPrincipal(
    aPrincipal,
    aOriginalURI,
    aMultiProcess,
    aRemoteSubframes,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE,
    aCurrentPrincipal,
    aIsSubframe
  ) {
    if (!aMultiProcess) {
      return NOT_REMOTE;
    }

    // We want to use the original URI for "about:" (except for "about:srcdoc"
    // and "about:blank") and "chrome://" scheme, so that we can properly
    // determine the remote type.
    let useOriginalURI;
    if (aOriginalURI.scheme == "about") {
      useOriginalURI = !["about:srcdoc", "about:blank"].includes(
        aOriginalURI.spec
      );
    } else {
      useOriginalURI = aOriginalURI.scheme == "chrome";
    }

    if (!useOriginalURI) {
      // We can't pick a process based on a system principal or expanded
      // principal.
      if (aPrincipal.isSystemPrincipal || aPrincipal.isExpandedPrincipal) {
        throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
      }

      // Null principals can be loaded in any remote process, but when
      // using fission we add the option to force them into the default
      // web process for better test coverage.
      if (aPrincipal.isNullPrincipal) {
        if (aOriginalURI.spec == "about:blank") {
          useOriginalURI = true;
        } else if (
          (aRemoteSubframes && useSeparateDataUriProcess) ||
          aPreferredRemoteType == NOT_REMOTE
        ) {
          return WEB_REMOTE_TYPE;
        }
        return aPreferredRemoteType;
      }
    }
    // We might care about the currently loaded URI. Pull it out of our current
    // principal. We never care about the current URI when working with a
    // non-content principal.
    let currentURI =
      aCurrentPrincipal && aCurrentPrincipal.isContentPrincipal
        ? Services.io.newURI(aCurrentPrincipal.spec)
        : null;

    return E10SUtils.getRemoteTypeForURIObject(
      useOriginalURI ? aOriginalURI : Services.io.newURI(aPrincipal.spec),
      aMultiProcess,
      aRemoteSubframes,
      aPreferredRemoteType,
      currentURI,
      aPrincipal,
      aIsSubframe,
      false, //aIsWorker
      aPrincipal.originAttributes
    );
  },

  getRemoteTypeForWorkerPrincipal(
    aPrincipal,
    aWorkerType,
    aIsMultiProcess,
    aIsFission,
    aPreferredRemoteType = DEFAULT_REMOTE_TYPE
  ) {
    if (aPrincipal.isExpandedPrincipal) {
      // Explicitly disallow expanded principals:
      // The worker principal is based on the worker script, an expanded principal
      // is not expected.
      throw new Error("Unexpected expanded principal worker");
    }

    if (
      aWorkerType === Ci.nsIE10SUtils.REMOTE_WORKER_TYPE_SERVICE &&
      !aPrincipal.isContentPrincipal
    ) {
      // Fails earlier on service worker with a non content principal.
      throw new Error("Unexpected system or null principal service worker");
    }

    if (!aIsMultiProcess) {
      // Return earlier when multiprocess is disabled.
      return NOT_REMOTE;
    }

    if (
      // We don't want to launch workers in a large allocation remote type,
      // change it to the web remote type (then getRemoteTypeForURIObject
      // may change it to an isolated one).
      aPreferredRemoteType === LARGE_ALLOCATION_REMOTE_TYPE ||
      // Similarly to the large allocation remote type, we don't want to
      // launch the shared worker in a web coop+coep remote type even if
      // was registered from a frame loaded in a child process with that
      // remote type.
      aPreferredRemoteType?.startsWith(WEB_REMOTE_COOP_COEP_TYPE_PREFIX)
    ) {
      aPreferredRemoteType = DEFAULT_REMOTE_TYPE;
    }

    // System principal shared workers are allowed to run in the main process
    // or in the privilegedabout child process. Early return the preferred remote type
    // if it is one where a system principal worked is allowed to run.
    if (
      aPrincipal.isSystemPrincipal &&
      SYSTEM_WORKERS_REMOTE_TYPES_ALLOWED.includes(aPreferredRemoteType)
    ) {
      return aPreferredRemoteType;
    }

    // Allow null principal shared workers to run in the same process type where they
    // have been registered (the preferredRemoteType), but return the DEFAULT_REMOTE_TYPE
    // if the preferred remote type was NOT_REMOTE.
    if (aPrincipal.isNullPrincipal) {
      return aPreferredRemoteType === NOT_REMOTE
        ? DEFAULT_REMOTE_TYPE
        : aPreferredRemoteType;
    }

    // Sanity check, there shouldn't be any system or null principal after this point.
    if (aPrincipal.isContentPrincipal) {
      // For content principal, get a remote type based on the worker principal URI
      // (which is based on the worker script url) and an initial preferredRemoteType
      // (only set for shared worker, based on the remote type where the shared worker
      // was registered from).
      return E10SUtils.getRemoteTypeForURIObject(
        aPrincipal.URI,
        aIsMultiProcess,
        aIsFission,
        aPreferredRemoteType,
        null,
        aPrincipal,
        false, // aIsSubFrame
        true, // aIsWorker
        aPrincipal.originAttributes
      );
    }

    // Throw explicitly if we were unable to get a remoteType for the worker.
    throw new Error(
      "Failed to get a remoteType for a non content principal worker"
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
      this.log().error(`Failed to serialize principal '${principal}' ${e}`);
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
        this.log().warn(
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
      this.log().error(
        `Failed to deserialize principal_b64 '${principal_b64}' ${e}`
      );
    }
    if (!fallbackPrincipalCallback) {
      this.log().warn(
        "No principal passed to deserializePrincipal and no fallbackPrincipalCallback"
      );
      return null;
    }
    return fallbackPrincipalCallback();
  },

  /**
   * Serialize cookieJarSettings.
   *
   * @param {nsICookieJarSettings} cookieJarSettings The cookieJarSettings to
   *   serialize.
   * @return {String} The base64 encoded cookieJarSettings data.
   */
  serializeCookieJarSettings(cookieJarSettings) {
    let serialized = null;
    if (cookieJarSettings) {
      try {
        serialized = serializationHelper.serializeToString(cookieJarSettings);
      } catch (e) {
        this.log().error(
          `Failed to serialize cookieJarSettings '${cookieJarSettings}' ${e}`
        );
      }
    }
    return serialized;
  },

  /**
   * Deserialize a base64 encoded cookieJarSettings
   *
   * @param {String} cookieJarSettings_b64 A base64 encoded serialized cookieJarSettings.
   * @return {nsICookieJarSettings} A deserialized cookieJarSettings.
   */
  deserializeCookieJarSettings(cookieJarSettings_b64) {
    let deserialized = null;
    if (cookieJarSettings_b64) {
      try {
        deserialized = serializationHelper.deserializeObject(
          cookieJarSettings_b64
        );
        deserialized.QueryInterface(Ci.nsICookieJarSettings);
      } catch (e) {
        this.log().error(
          `Failed to deserialize cookieJarSettings_b64 '${cookieJarSettings_b64}' ${e}`
        );
      }
    }
    return deserialized;
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
        this.log().error(
          `Failed to serialize referrerInfo '${referrerInfo}' ${e}`
        );
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
        this.log().error(
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
        stack.push(...bc.children);
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
   * There are various types of remote types that are for web content processes, but
   * they all start with "web". The C++ version of this method is
   * mozilla::dom::IsWebRemoteType().
   */
  isWebRemoteType(aRemoteType) {
    return aRemoteType.startsWith(WEB_REMOTE_TYPE);
  },

  /**
   * Assemble or predict originAttributes from available arguments.
   */
  predictOriginAttributes({
    window,
    browser,
    userContextId,
    geckoViewSessionContextId,
    privateBrowsingId,
  }) {
    if (browser) {
      if (browser.browsingContext) {
        return browser.browsingContext.originAttributes;
      }
      if (!window) {
        window = browser.contentDocument?.defaultView;
      }
      if (!userContextId) {
        userContextId = browser.getAttribute("usercontextid") || 0;
      }
      if (!geckoViewSessionContextId) {
        geckoViewSessionContextId =
          browser.getAttribute("geckoViewSessionContextId") || "";
      }
    }

    if (window && !privateBrowsingId) {
      privateBrowsingId = window.browsingContext.usePrivateBrowsing ? 1 : 0;
    }
    return { privateBrowsingId, userContextId, geckoViewSessionContextId };
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
