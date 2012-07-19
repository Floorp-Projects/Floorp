/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let modules = {
  // about:blank has some bad loading behavior we can avoid, if we use an alias
  empty: {
    uri: "about:blank",
    privileged: false
  },
  fennec: {
    uri: "chrome://browser/content/about.xhtml",
    privileged: true
  },
  // about:firefox is an alias for about:fennec
  get firefox() this.fennec,

  firstrun: {
    uri: "chrome://browser/content/firstrun/firstrun.xhtml",
    privileged: true
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
      // Setting the owner to null means that we'll go through the normal
      // path in GetChannelPrincipal and create a codebase principal based
      // on the channel's originalURI
      channel.owner = null;
    }

    channel.originalURI = aURI;

    return channel;
  }
};

function AboutEmpty() {}
AboutEmpty.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{433d2d75-5923-49b0-854d-f37267b03dc7}")
}

function AboutFennec() {}
AboutFennec.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{842a6d11-b369-4610-ba66-c3b5217e82be}")
}

function AboutFirefox() {}
AboutFirefox.prototype = {
  __proto__: AboutGeneric.prototype,
  classID: Components.ID("{dd40c467-d206-4f22-9215-8fcc74c74e38}")  
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

const components = [AboutEmpty, AboutFennec, AboutRights,
                    AboutCertError, AboutFirefox, AboutHome, AboutBlocked];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
