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
 * moz-page-thumb://thumbnail/?url=http%3A%2F%2Fwww.mozilla.org%2F&revision=XX
 *
 * This URL requests an image for 'http://www.mozilla.org/'.
 * The value of the revision key may change when the stored thumbnail changes.
 */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
  "resource://gre/modules/FileUtils.jsm");

const SUBSTITUTING_URL_CID = "{dea9657c-18cf-4984-bde9-ccef5d8ab473}";

/**
 * Implements the thumbnail protocol handler responsible for moz-page-thumb: URLs.
 */
function Protocol() {
}

Protocol.prototype = {
  /**
   * The scheme used by this protocol.
   */
  get scheme() {
    return PageThumbs.scheme;
  },

  /**
   * The default port for this protocol (we don't support ports).
   */
  get defaultPort() {
    return -1;
  },

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
    let uri = Components.classesByID[SUBSTITUTING_URL_CID].createInstance(Ci.nsIURL);
    uri.spec = aSpec;
    return uri;
  },

  /**
   * Constructs a new channel from the given URI for this protocol handler.
   * @param aURI The URI for which to construct a channel.
   * @param aLoadInfo The Loadinfo which to use on the channel.
   * @return The newly created channel.
   */
  newChannel2: function Proto_newChannel2(aURI, aLoadInfo) {
    let {file} = aURI.QueryInterface(Ci.nsIFileURL);
    let fileuri = Services.io.newFileURI(file);
    let channel = Services.io.newChannelFromURIWithLoadInfo(fileuri, aLoadInfo);
    channel.originalURI = aURI;
    return channel;
  },

  newChannel: function Proto_newChannel(aURI) {
    return this.newChannel2(aURI, null);
  },

  /**
   * Decides whether to allow a blacklisted port.
   * @return Always false, we'll never allow ports.
   */
  allowPort: () => false,

  // nsISubstitutingProtocolHandler methods

  /*
   * Substituting the scheme and host isn't enough, we also transform the path.
   * So declare no-op implementations for (get|set|has)Substitution methods and
   * do all the work in resolveURI.
   */

  setSubstitution(root, baseURI) {},

  getSubstitution(root) {
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  hasSubstitution(root) {
    return false;
  },

  resolveURI(resURI) {
    let {url} = parseURI(resURI);
    let path = PageThumbsStorage.getFilePathForURL(url);
    return OS.Path.toFileURI(path);
  },

  // xpcom machinery
  classID: Components.ID("{5a4ae9b5-f475-48ae-9dce-0b4c1d347884}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler,
                                         Ci.nsISubstitutingProtocolHandler])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Protocol]);

/**
 * Parses a given URI and extracts all parameters relevant to this protocol.
 * @param aURI The URI to parse.
 * @return The parsed parameters.
 */
function parseURI(aURI) {
  if (aURI.host != PageThumbs.staticHost)
    throw Cr.NS_ERROR_NOT_AVAILABLE;

  let {query} = aURI.QueryInterface(Ci.nsIURL);
  let params = {};

  query.split("&").forEach(function (aParam) {
    let [key, value] = aParam.split("=").map(decodeURIComponent);
    params[key.toLowerCase()] = value;
  });

  return params;
}
