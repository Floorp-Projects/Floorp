/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["WebRequestCommon"];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

let WebRequestCommon = {
  typeForPolicyType(type) {
    switch (type) {
      case Ci.nsIContentPolicy.TYPE_DOCUMENT: return "main_frame";
      case Ci.nsIContentPolicy.TYPE_SUBDOCUMENT: return "sub_frame";
      case Ci.nsIContentPolicy.TYPE_STYLESHEET: return "stylesheet";
      case Ci.nsIContentPolicy.TYPE_SCRIPT: return "script";
      case Ci.nsIContentPolicy.TYPE_IMAGE: return "image";
      case Ci.nsIContentPolicy.TYPE_OBJECT: return "object";
      case Ci.nsIContentPolicy.TYPE_OBJECT_SUBREQUEST: return "object";
      case Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST: return "xmlhttprequest";
      default: return "other";
    }
  },

  typeMatches(policyType, filterTypes) {
    if (filterTypes === null) {
      return true;
    }

    return filterTypes.indexOf(this.typeForPolicyType(policyType)) != -1;
  },

  urlMatches(uri, urlFilters) {
    if (urlFilters === null) {
      return true;
    }

    for (let urlRegexp of urlFilters) {
      if (urlRegexp.test(uri.spec)) {
        return true;
      }
    }
    return false;
  }
};
