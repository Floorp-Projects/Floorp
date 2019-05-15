/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebRequestCommon"];

/* exported WebRequestCommon */

var WebRequestCommon = {
  typeForPolicyType(type) {
    switch (type) {
      case Ci.nsIContentPolicy.TYPE_DOCUMENT: return "main_frame";
      case Ci.nsIContentPolicy.TYPE_SUBDOCUMENT: return "sub_frame";
      case Ci.nsIContentPolicy.TYPE_STYLESHEET: return "stylesheet";
      case Ci.nsIContentPolicy.TYPE_SCRIPT: return "script";
      case Ci.nsIContentPolicy.TYPE_IMAGE: return "image";
      case Ci.nsIContentPolicy.TYPE_OBJECT: return "object";
      case Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST: return "object_subrequest";
      case Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST: return "xmlhttprequest";
      // TYPE_FETCH returns xmlhttprequest for cross-browser compatibility.
      case Ci.nsIContentPolicy.TYPE_FETCH: return "xmlhttprequest";
      case Ci.nsIContentPolicy.TYPE_XBL: return "xbl";
      case Ci.nsIContentPolicy.TYPE_XSLT: return "xslt";
      case Ci.nsIContentPolicy.TYPE_PING: return "ping";
      case Ci.nsIContentPolicy.TYPE_BEACON: return "beacon";
      case Ci.nsIContentPolicy.TYPE_DTD: return "xml_dtd";
      case Ci.nsIContentPolicy.TYPE_FONT: return "font";
      case Ci.nsIContentPolicy.TYPE_MEDIA: return "media";
      case Ci.nsIContentPolicy.TYPE_WEBSOCKET: return "websocket";
      case Ci.nsIContentPolicy.TYPE_CSP_REPORT: return "csp_report";
      case Ci.nsIContentPolicy.TYPE_IMAGESET: return "imageset";
      case Ci.nsIContentPolicy.TYPE_WEB_MANIFEST: return "web_manifest";
      default: return "other";
    }
  },

  typeMatches(policyType, filterTypes) {
    if (filterTypes === null) {
      return true;
    }

    return filterTypes.includes(this.typeForPolicyType(policyType));
  },

  urlMatches(uri, urlFilter) {
    if (urlFilter === null) {
      return true;
    }

    return urlFilter.matches(uri);
  },
};
