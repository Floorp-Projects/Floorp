/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

// Shared functions for files testing Bug 593815

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

function HelperAppDlg() { }
HelperAppDlg.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),
  classID: Components.ID("49456eda-4dc4-4d1a-b8e8-0b94749bf99e"),
  show: function (launcher, ctx, reason) {
    launcher.MIMEInfo.preferredAction = Ci.nsIMIMEInfo.saveToDisk;
    launcher.launchWithApplication(null, false);
  }
}


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HelperAppDlg]);
