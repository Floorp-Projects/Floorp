/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function base64UrlDecode(s) {
  s = s.replace(/-/g, "+");
  s = s.replace(/_/g, "/");

  // Replace padding if it was stripped by the sender.
  // See http://tools.ietf.org/html/rfc4648#section-4
  switch (s.length % 4) {
    case 0:
      break; // No pad chars in this case
    case 2:
      s += "==";
      break; // Two pad chars
    case 3:
      s += "=";
      break; // One pad char
    default:
      throw new InputException("Illegal base64url string!");
  }

  // With correct padding restored, apply the standard base64 decoder
  return atob(s);
}
