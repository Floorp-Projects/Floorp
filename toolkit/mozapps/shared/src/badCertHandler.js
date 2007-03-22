#if 0
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Update Service.
 *
 * The Initial Developer of the Original Code is Ben Goodger.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Daniel Veditz <dveditz@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#endif

/**
 * Only allow built-in certs for HTTPS connections.  See bug 340198.
 */
function checkCert(channel) {
  if (!channel.originalURI.schemeIs("https"))  // bypass
    return;

  const Ci = Components.interfaces;  
  var cert =
      channel.securityInfo.QueryInterface(Ci.nsISSLStatusProvider).
      SSLStatus.QueryInterface(Ci.nsISSLStatus).serverCert;

  var issuer = cert.issuer;
  while (issuer && !cert.equals(issuer)) {
    cert = issuer;
    issuer = cert.issuer;
  }

  if (!issuer || issuer.tokenName != "Builtin Object Token")
    throw "cert issuer is not built-in";
}

/**
 * This class implements nsIBadCertListener.  It's job is to prevent "bad cert"
 * security dialogs from being shown to the user.  It is better to simply fail
 * if the certificate is bad. See bug 304286.
 */
function BadCertHandler() {
}
BadCertHandler.prototype = {

  // nsIBadCertListener
  confirmUnknownIssuer: function(socketInfo, cert, certAddType) {
    LOG("EM BadCertHandler: Unknown issuer");
    return false;
  },

  confirmMismatchDomain: function(socketInfo, targetURL, cert) {
    LOG("EM BadCertHandler: Mismatched domain");
    return false;
  },

  confirmCertExpired: function(socketInfo, cert) {
    LOG("EM BadCertHandler: Expired certificate");
    return false;
  },

  notifyCrlNextupdate: function(socketInfo, targetURL, cert) {
  },

  // nsIChannelEventSink
  onChannelRedirect: function(oldChannel, newChannel, flags) {
    // make sure the certificate of the old channel checks out before we follow
    // a redirect from it.  See bug 340198.
    checkCert(oldChannel);
  },

  // nsIInterfaceRequestor
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIBadCertListener) &&
        !iid.equals(Components.interfaces.nsIChannelEventSink) &&
        !iid.equals(Components.interfaces.nsIInterfaceRequestor) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
