/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Utils"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyServiceGetter(this, "serializationHelper",
                                   "@mozilla.org/network/serialization-helper;1",
                                   "nsISerializationHelper");

this.Utils = Object.freeze({
  makeURI: function (url) {
    return Services.io.newURI(url, null, null);
  },

  makeInputStream: function (aString) {
    let stream = Cc["@mozilla.org/io/string-input-stream;1"].
                 createInstance(Ci.nsISupportsCString);
    stream.data = aString;
    return stream; // XPConnect will QI this to nsIInputStream for us.
  },

  /**
   * Returns true if the |url| passed in is part of the given root |domain|.
   * For example, if |url| is "www.mozilla.org", and we pass in |domain| as
   * "mozilla.org", this will return true. It would return false the other way
   * around.
   */
  hasRootDomain: function (url, domain) {
    let host;

    try {
      host = this.makeURI(url).host;
    } catch (e) {
      // The given URL probably doesn't have a host.
      return false;
    }

    let index = host.indexOf(domain);
    if (index == -1)
      return false;

    if (host == domain)
      return true;

    let prevChar = host[index - 1];
    return (index == (host.length - domain.length)) &&
           (prevChar == "." || prevChar == "/");
  },

  shallowCopy: function (obj) {
    let retval = {};

    for (let key of Object.keys(obj)) {
      retval[key] = obj[key];
    }

    return retval;
  },

  /**
   * Serialize principal data.
   *
   * @param {nsIPrincipal} principal The principal to serialize.
   * @return {String} The base64 encoded principal data.
   */
  serializePrincipal(principal) {
    if (!principal)
      return null;

    return serializationHelper.serializeToString(principal);
  },

  /**
   * Deserialize a base64 encoded principal (serialized with
   * Utils::serializePrincipal).
   *
   * @param {String} principal_b64 A base64 encoded serialized principal.
   * @return {nsIPrincipal} A deserialized principal.
   */
  deserializePrincipal(principal_b64) {
    if (!principal_b64)
      return null;

    let principal = serializationHelper.deserializeObject(principal_b64);
    principal.QueryInterface(Ci.nsIPrincipal);

    return principal;
  }
});
