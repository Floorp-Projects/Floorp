/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
  },
  reader: {
    uri: "chrome://browser/content/aboutReader.html",
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
      // Setting the owner to null means that we'll go through the normal
      // path in GetChannelPrincipal and create a codebase principal based
      // on the channel's originalURI
      channel.owner = null;
    }

    channel.originalURI = aURI;

    return channel;
  }
};

const components = [AboutRedirector];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
