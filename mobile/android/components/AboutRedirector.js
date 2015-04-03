/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
    uri: AppConstants.MOZ_OFFICIAL_BRANDING ?
      "chrome://browser/content/aboutRights.xhtml" :
      "chrome://global/content/aboutRights-unbranded.xhtml",
    privileged: false
  },
  blocked: {
    uri: "chrome://browser/content/blockedSite.xhtml",
    privileged: false,
    hide: true
  },
  certerror: {
    uri: "chrome://browser/content/aboutCertError.xhtml",
    privileged: false,
    hide: true
  },
  home: {
    uri: "chrome://browser/content/aboutHome.xhtml",
    privileged: false
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
    uri: "chrome://global/content/reader/aboutReader.html",
    privileged: false,
    dontLink: true,
    hide: true
  },
  feedback: {
    uri: "chrome://browser/content/aboutFeedback.xhtml",
    privileged: true
  },
  privatebrowsing: {
    uri: "chrome://browser/content/aboutPrivateBrowsing.xhtml",
    privileged: true
  },
}

if (AppConstants.MOZ_SERVICES_HEALTHREPORT) {
  modules['healthreport'] = {
    uri: "chrome://browser/content/aboutHealthReport.xhtml",
    privileged: true
  };
}
if (AppConstants.MOZ_DEVICES) {
  modules['devices'] = {
    uri: "chrome://browser/content/aboutDevices.xhtml",
    privileged: true
  };
}
if (AppConstants.NIGHTLY_BUILD) {
  modules['passwords'] = {
    uri: "chrome://browser/content/aboutPasswords.xhtml",
    privileged: true
  };
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
    if (moduleInfo.dontLink)
      flags = flags | Ci.nsIAboutModule.MAKE_UNLINKABLE;

    return flags | Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel: function(aURI, aLoadInfo) {
    let moduleInfo = this._getModuleInfo(aURI);

    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

    var newURI = ios.newURI(moduleInfo.uri, null, null);

    var channel = ios.newChannelFromURIWithLoadInfo(newURI, aLoadInfo);

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
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
