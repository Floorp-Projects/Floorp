const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/protection/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/protection/js/lang.js",

  "chrome://global/content/protection/moz/preferences.js",
  "chrome://global/content/protection/moz/filesystem.js",
  "chrome://global/content/protection/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/protection/map.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function ProtectionTableSite() {
  G_Map.call(this);
  this.debugZone = "trtable-site";
  this.ioService_ = Cc["@mozilla.org/network/io-service;1"]
                      .getService(Ci.nsIIOService);
}

ProtectionTableSite.inherits(G_Map);

/**
 * Look up a URL in a site DB
 *
 * @returns Boolean true if URL matches in table, we normalize based on
 *                  part of the path
 */
ProtectionTableSite.prototype.find = function(url) {
  var nsIURI = this.ioService_.newURI(url, null, null);
  var host = nsIURI.asciiHost;
  var hostComponents = host.split(".");

  var path = nsIURI.path;
  var pathComponents = path.split("/");

  // We don't have a good way to convert a fully specified URL into a
  // site.  We try host name components with or without the first 
  // path component.
  for (var i = 0; i < hostComponents.length - 1; i++) {
    host = hostComponents.slice(i).join(".") + "/";
    var val = this.find_(host);
    G_Debug(this, "Checking: " + host + " : " + val);
    if (val)
      return true;

    // The path starts with a "/", so we are interested in the second path
    // component if it is available
    if (pathComponents.length >= 2) {
      host = host + pathComponents[1] + "/";
      var val = this.find_(host);
      G_Debug(this, "Checking: " + host + " : " + val);
      if (val)
        return true;
    }
  }

  return false;
}

// Module object
function ProtectionTableSiteMod() {
  this.firstTime = true;
  this.cid = Components.ID("{1e48f217-34e4-44a3-9b4a-9b66ed0a1201}");
  this.progid = "@mozilla.org/protection/protectiontable;1?type=site";
}

ProtectionTableSiteMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Protection Table Site Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

ProtectionTableSiteMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

ProtectionTableSiteMod.prototype.canUnload = function(compMgr) {
  return true;
}

ProtectionTableSiteMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new ProtectionTableSite()).QueryInterface(iid);
  }
};

var SiteModInst = new ProtectionTableSiteMod();

function NSGetModule(compMgr, fileSpec) {
  return SiteModInst;
}
