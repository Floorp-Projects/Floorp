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
 * The Original Code is About:FirstRun.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ryan Flint <rflint@mozilla.com>
 *   Justin Dolske <dolske@mozilla.com>
 *   Gavin Sharp <gavin@gavinsharp.com>
 *   Steffen Wilberg <steffen.wilberg@web.de>
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
const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let modules = {
  // about:
  "": {
    uri: "chrome://browser/content/about.xhtml",
    privileged: true
  },

  // about:fennec and about:firefox are aliases for about:,
  // but hidden from about:about
  fennec: {
    uri: "chrome://browser/content/about.xhtml",
    privileged: true,
    hide: true
  },
  get firefox() this.fennec,

  // about:blank has some bad loading behavior we can avoid, if we use an alias
  empty: {
    uri: "about:blank",
    privileged: false,
    hide: true
  },

  rights: {
#ifdef MOZ_OFFICIAL_BRANDING
    uri: "chrome://browser/content/aboutRights.xhtml",
#else
    uri: "chrome://global/content/aboutRights-unbranded.xhtml",
#endif
    privileged: false
  },
  blocked: {
    uri: "chrome://browser/content/blockedSite.xhtml",
    privileged: true,
    hide: true
  },
  certerror: {
    uri: "chrome://browser/content/aboutCertError.xhtml",
    privileged: true,
    hide: true
  },
  home: {
    uri: "chrome://browser/content/aboutHome.xhtml",
    privileged: true
  },
  apps: {
    uri: "chrome://browser/content/aboutApps.xhtml",
    privileged: true
  },
  downloads: {
    uri: "chrome://browser/content/aboutDownloads.xhtml",
    privileged: true
  }
}

function AboutRedirector() {}
AboutRedirector.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),
  classID: Components.ID("{322ba47e-7047-4f71-aebf-cb7d69325cd9}"),

  _getModuleInfo: function (aURI) {
    let moduleName = aURI.path.replace(/[?#].*/, "").toLowerCase();
    return modules[moduleName];
  },

  // nsIAboutModule
  getURIFlags: function(aURI) {
    let flags;
    let moduleInfo = this._getModuleInfo(aURI);
    if (moduleInfo.hide)
      flags = Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT;

    return flags | Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel: function(aURI) {
    let moduleInfo = this._getModuleInfo(aURI);

    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

    var channel = ios.newChannel(moduleInfo.uri, null, null);
    
    if (!moduleInfo.privileged) {
      // drop chrome privileges
      let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].
                   getService(Ci.nsIScriptSecurityManager);
      let principal = secMan.getCodebasePrincipal(aURI);
      channel.owner = principal;
    }

    channel.originalURI = aURI;

    return channel;
  }
};

const components = [AboutRedirector];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
