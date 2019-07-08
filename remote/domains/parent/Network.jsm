/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Network"];

const { Domain } = ChromeUtils.import(
  "chrome://remote/content/domains/Domain.jsm"
);
const { NetworkObserver } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/network/NetworkObserver.jsm"
);

const LOAD_CAUSE_STRINGS = {
  [Ci.nsIContentPolicy.TYPE_INVALID]: "Invalid",
  [Ci.nsIContentPolicy.TYPE_OTHER]: "Other",
  [Ci.nsIContentPolicy.TYPE_SCRIPT]: "Script",
  [Ci.nsIContentPolicy.TYPE_IMAGE]: "Img",
  [Ci.nsIContentPolicy.TYPE_STYLESHEET]: "Stylesheet",
  [Ci.nsIContentPolicy.TYPE_OBJECT]: "Object",
  [Ci.nsIContentPolicy.TYPE_DOCUMENT]: "Document",
  [Ci.nsIContentPolicy.TYPE_SUBDOCUMENT]: "Subdocument",
  [Ci.nsIContentPolicy.TYPE_REFRESH]: "Refresh",
  [Ci.nsIContentPolicy.TYPE_XBL]: "Xbl",
  [Ci.nsIContentPolicy.TYPE_PING]: "Ping",
  [Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST]: "Xhr",
  [Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST]: "ObjectSubdoc",
  [Ci.nsIContentPolicy.TYPE_DTD]: "Dtd",
  [Ci.nsIContentPolicy.TYPE_FONT]: "Font",
  [Ci.nsIContentPolicy.TYPE_MEDIA]: "Media",
  [Ci.nsIContentPolicy.TYPE_WEBSOCKET]: "Websocket",
  [Ci.nsIContentPolicy.TYPE_CSP_REPORT]: "Csp",
  [Ci.nsIContentPolicy.TYPE_XSLT]: "Xslt",
  [Ci.nsIContentPolicy.TYPE_BEACON]: "Beacon",
  [Ci.nsIContentPolicy.TYPE_FETCH]: "Fetch",
  [Ci.nsIContentPolicy.TYPE_IMAGESET]: "Imageset",
  [Ci.nsIContentPolicy.TYPE_WEB_MANIFEST]: "WebManifest",
};

class Network extends Domain {
  constructor(session) {
    super(session);
    this.enabled = false;

    this._onRequest = this._onRequest.bind(this);
  }

  destructor() {
    this.disable();

    super.destructor();
  }

  enable() {
    if (this.enabled) {
      return;
    }
    this.enabled = true;
    this._networkObserver = new NetworkObserver();
    const { browser } = this.session.target;
    this._networkObserver.startTrackingBrowserNetwork(browser);
    this._networkObserver.on("request", this._onRequest);
  }

  disable() {
    if (!this.enabled) {
      return;
    }
    const { browser } = this.session.target;
    this._networkObserver.stopTrackingBrowserNetwork(browser);
    this._networkObserver.off("request", this._onRequest);
    this._networkObserver.dispose();
    this.enabled = false;
  }

  _onRequest(eventName, httpChannel, data) {
    const topFrame = getLoadContext(httpChannel).topFrameElement;
    const request = {
      url: httpChannel.URI.spec,
      urlFragment: undefined,
      method: httpChannel.requestMethod,
      headers: [],
      postData: undefined,
      hasPostData: false,
      mixedContentType: undefined,
      initialPriority: undefined,
      referrerPolicy: undefined,
      isLinkPreload: false,
    };
    let loaderId = undefined;
    let causeType = Ci.nsIContentPolicy.TYPE_OTHER;
    let causeUri = topFrame.currentURI.spec;
    if (httpChannel.loadInfo) {
      causeType = httpChannel.loadInfo.externalContentPolicyType;
      const { loadingPrincipal } = httpChannel.loadInfo;
      if (loadingPrincipal && loadingPrincipal.URI) {
        causeUri = loadingPrincipal.URI.spec;
      }
      if (causeType == Ci.nsIContentPolicy.TYPE_DOCUMENT) {
        // Puppeteer expect this specialy of CDP where loaderId = requestId
        // for the toplevel document request
        loaderId = String(httpChannel.channelId);
      }
    }
    this.emit("Network.requestWillBeSent", {
      requestId: String(httpChannel.channelId),
      loaderId,
      documentURL: causeUri,
      request,
      timestamp: undefined,
      wallTime: undefined,
      initiator: undefined,
      redirectResponse: undefined,
      type: LOAD_CAUSE_STRINGS[causeType] || "unknown",
      frameId: topFrame.outerWindowID,
      hasUserGesture: undefined,
    });
  }
}

function getLoadContext(httpChannel) {
  let loadContext = null;
  try {
    if (httpChannel.notificationCallbacks) {
      loadContext = httpChannel.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  try {
    if (!loadContext && httpChannel.loadGroup) {
      loadContext = httpChannel.loadGroup.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    }
  } catch (e) {}
  return loadContext;
}
