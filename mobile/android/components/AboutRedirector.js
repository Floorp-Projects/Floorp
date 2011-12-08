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

  // about:fennec and about:firefox are aliases for about:
  get fennec() this[""],
  get firefox() this[""],

  // about:blank has some bad loading behavior we can avoid, if we use an alias
  empty: {
    uri: "about:blank",
    privileged: false
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
    privileged: true
  },
  certerror: {
    uri: "chrome://browser/content/aboutCertError.xhtml",
    privileged: true
  },
  home: {
    uri: "chrome://browser/content/aboutHome.xhtml",
    privileged: true
  }
}

function AboutGeneric() {}
AboutGeneric.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),

  _getModuleInfo: function (aURI) {
    let moduleName = aURI.path.replace(/[?#].*/, "").toLowerCase();
    return modules[moduleName];
  },

  // nsIAboutModule
  getURIFlags: function(aURI) {
    return Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel: function(aURI) {
    let moduleInfo = this._getModuleInfo(aURI);

    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

    var channel = ios.newChannel(moduleInfo.uri, null, null);
    
    if (!moduleInfo.privileged) {
      let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].
                   getService(Ci.nsIScriptSecurityManager);
      let principal = secMan.getCodebasePrincipal(aURI);
      channel.owner = principal;
    }

    channel.originalURI = aURI;

    return channel;
  }
};

function About() {}
About.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{322ba47e-7047-4f71-aebf-cb7d69325cd9}")
}

function AboutEmpty() {}
AboutEmpty.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{433d2d75-5923-49b0-854d-f37267b03dc7}")
}

function AboutRights() {}
AboutRights.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{3b988fbf-ec97-4e1c-a5e4-573d999edc9c}")
}

function AboutCertError() {}
AboutCertError.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{972efe64-8ac0-4e91-bdb0-22835d987815}")
}

function AboutHome() {}
AboutHome.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{b071364f-ab68-4669-a9db-33fca168271a}")
}

function AboutBlocked() {}
AboutBlocked.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{88fd40b6-c5c2-4120-9238-f2cb9ff98928}")
}

const components = [About, AboutEmpty, AboutRights, AboutCertError, AboutHome, AboutBlocked];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
