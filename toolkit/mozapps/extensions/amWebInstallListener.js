/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Extension Manager.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Townsend <dtownsend@oxymoronical.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

/**
 * This is a default implementation of amIWebInstallListener that should work
 * for most applications but can be overriden. It notifies the observer service
 * about blocked installs. For normal installs it pops up an install
 * confirmation when all the add-ons have been downloaded.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

/**
 * Logs a warning message
 *
 * @param   str
 *          The string to log
 */
function WARN(str) {
  dump("*** addons.weblistener: " + str + "\n");
}

/**
 * Creates a new installer to monitor downloads and prompt to install when
 * ready
 *
 * @param    window
 *           The window that started the installations
 * @param    url
 *           The URL that started the installations
 * @installs installs
 *           An array of AddonInstalls
 */
function Installer(window, url, installs) {
  this.window = window;
  this.url = url;
  this.downloads = installs;
  this.installs = [];

  this.bundle = Cc["@mozilla.org/intl/stringbundle;1"].
                getService(Ci.nsIStringBundleService).
                createBundle("chrome://mozapps/locale/extensions/extensions.properties");

  this.count = installs.length;
  installs.forEach(function(install) {
    install.addListener(this);

    // Might already be a local file
    if (install.state == AddonManager.STATE_DOWNLOADED)
      this.onDownloadEnded(install);
    else
      install.install();
  }, this);
}

Installer.prototype = {
  window: null,
  downloads: null,
  installs: null,
  count: null,

  /**
   * Checks if all downloads are now complete and if so prompts to install.
   */
  checkAllDownloaded: function() {
    if (--this.count > 0)
      return;

    // Maybe none of the downloads were sucessful
    if (this.installs.length == 0)
      return;

    let args = {};
    args.url = this.url;
    args.installs = this.installs;
    args.wrappedJSObject = args;

    Services.ww.openWindow(this.window, "chrome://mozapps/content/xpinstall/xpinstallConfirm.xul",
                           null, "chrome,modal,centerscreen", args);
  },

  onDownloadCancelled: function(install) {
    install.removeListener(this);

    this.checkAllDownloaded();
  },

  onDownloadFailed: function(install, error) {
    install.removeListener(this);

    // TODO show some better error
    Services.prompt.alert(this.window, "Download Failed", "The download of " + install.sourceURL + " failed: " + error);
    this.checkAllDownloaded();
  },

  onDownloadEnded: function(install) {
    install.removeListener(this);

    if (install.addon.appDisabled) {
      // App disabled items are not compatible
      install.cancel();

      let title = null;
      let text = null;

      let problems = "";
      if (!install.addon.isCompatible)
        problems += "incompatible, ";
      if (!install.addon.providesUpdatesSecurely)
        problems += "insecure updates, ";
      if (install.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
        problems += "blocklisted, ";
        title = bundle.GetStringFromName("blocklistedInstallTitle2");
        text = this.bundle.formatStringFromName("blocklistedInstallMsg2",
                                                [install.addon.name], 1);
      }
      problems = problems.substring(0, problems.length - 2);
      WARN("Not installing " + install.addon.id + " because of the following: " + problems);

      title = this.bundle.GetStringFromName("incompatibleTitle2", 1);
      text = this.bundle.formatStringFromName("incompatibleMessage",
                                              [install.addon.name,
                                               install.addon.version,
                                               Services.appinfo.name,
                                               Services.appinfo.version], 4);
      Services.prompt.alert(this.window, title, text);
    }
    else {
      this.installs.push(install);
    }

    this.checkAllDownloaded();
    return false;
  },
};

function extWebInstallListener() {
}

extWebInstallListener.prototype = {
  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallBlocked: function(window, uri, installs) {
    let info = {
      originatingWindow: window,
      originatingURI: uri,
      installs: installs,

      install: function() {
        dump("Start installs\n");
        new Installer(this.originatingWindow, this.originatingURI, this.installs);
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallInfo])
    };
    Services.obs.notifyObservers(info, "addon-install-blocked", null);

    return false;
  },

  /**
   * @see amIWebInstallListener.idl
   */
  onWebInstallRequested: function(window, uri, installs) {
    new Installer(window, uri, installs);

    // We start the installs ourself
    return false;
  },

  classDescription: "XPI Install Handler",
  contractID: "@mozilla.org/addons/web-install-listener;1",
  classID: Components.ID("{0f38e086-89a3-40a5-8ffc-9b694de1d04a}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallListener])
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([extWebInstallListener]);
