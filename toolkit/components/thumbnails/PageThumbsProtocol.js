/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PageThumbsProtocol.js
 *
 * This file implements the moz-page-thumb:// protocol and the corresponding
 * channel delivering cached thumbnails.
 *
 * URL structure:
 *
 * moz-page-thumb://thumbnail?url=http%3A%2F%2Fwww.mozilla.org%2F
 *
 * This URL requests an image for 'http://www.mozilla.org/'.
 */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

/**
 * Implements the thumbnail protocol handler responsible for moz-page-thumb: URIs.
 */
function Protocol() {
}

Protocol.prototype = {
  /**
   * The scheme used by this protocol.
   */
  get scheme() PageThumbs.scheme,

  /**
   * The default port for this protocol (we don't support ports).
   */
  get defaultPort() -1,

  /**
   * The flags specific to this protocol implementation.
   */
  get protocolFlags() {
    return Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
           Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE |
           Ci.nsIProtocolHandler.URI_NORELATIVE |
           Ci.nsIProtocolHandler.URI_NOAUTH;
  },

  /**
   * Creates a new URI object that is suitable for loading by this protocol.
   * @param aSpec The URI string in UTF8 encoding.
   * @param aOriginCharset The charset of the document from which the URI originated.
   * @return The newly created URI.
   */
  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  /**
   * Constructs a new channel from the given URI for this protocol handler.
   * @param aURI The URI for which to construct a channel.
   * @return The newly created channel.
   */
  newChannel: function Proto_newChannel(aURI) {
    let {url} = parseURI(aURI);
    let file = PageThumbsStorage.getFilePathForURL(url);
    let fileuri = Services.io.newFileURI(new FileUtils.File(file));
    return Services.io.newChannelFromURI2(fileuri,
                                          null,      // aLoadingNode
                                          Services.scriptSecurityManager.getSystemPrincipal(),
                                          null,      // aTriggeringPrincipal
                                          Ci.nsILoadInfo.SEC_NORMAL,
                                          Ci.nsIContentPolicy.TYPE_IMAGE);
  },

  /**
   * Decides whether to allow a blacklisted port.
   * @return Always false, we'll never allow ports.
   */
  allowPort: function () false,

  classID: Components.ID("{5a4ae9b5-f475-48ae-9dce-0b4c1d347884}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Protocol]);

/**
 * Parses a given URI and extracts all parameters relevant to this protocol.
 * @param aURI The URI to parse.
 * @return The parsed parameters.
 */
function parseURI(aURI) {
  let {scheme, staticHost} = PageThumbs;
  let re = new RegExp("^" + scheme + "://" + staticHost + ".*?\\?");
  let query = aURI.spec.replace(re, "");
  let params = {};

  query.split("&").forEach(function (aParam) {
    let [key, value] = aParam.split("=").map(decodeURIComponent);
    params[key.toLowerCase()] = value;
  });

  return params;
}
