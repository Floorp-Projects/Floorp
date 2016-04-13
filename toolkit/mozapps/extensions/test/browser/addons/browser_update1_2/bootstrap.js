/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function install(data, reason) {}
function startup(data, reason) {
  Components.utils.import("resource://gre/modules/Services.jsm");
  Services.ppmm.loadProcessScript(
    "resource://my-addon/frame-script.js", false);
}
function shutdown(data, reason) {}
function uninstall(data, reason) {}
