/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* exported startup, shutdown, install, uninstall */

ChromeUtils.import("resource://gre/modules/Services.jsm");

const resProto = Cc["@mozilla.org/network/protocol;1?name=resource"]
  .getService(Ci.nsISubstitutingProtocolHandler);

function install(data, reason) {}
function startup(data, reason) {
  resProto.setSubstitution("my-addon", data.resourceURI);
  Services.ppmm.loadProcessScript(
    "resource://my-addon/frame-script.js", false);
}
function shutdown(data, reason) {
  resProto.setSubstitution("my-addon", null);
}
function uninstall(data, reason) {}
