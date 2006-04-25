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
  "chrome://global/content/protection/url-canonicalizer.js"
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function ProtectionTableUrl() {
  G_Map.call(this);
  this.debugZone = "trtable-url";
}

ProtectionTableUrl.inherits(G_Map);

/**
 * Look up a URL in a URL table
 *
 * @returns Boolean true if the canonicalized url is in the table
 */
ProtectionTableUrl.prototype.find = function(url) {
  var canonicalized = PROT_URLCanonicalizer.canonicalizeURL_(url);
  // Uncomment for debugging
  G_Debug(this, "Looking up: " + url + " (" + canonicalized + ")");
  return this.find_(canonicalized);
}


// Module object
function ProtectionTableUrlMod() {
  this.firstTime = true;
  this.cid = Components.ID("{43399ee0-da0b-46a8-9541-08721265981c}");
  this.progid = "@mozilla.org/protection/protectiontable;1?type=url";
}

ProtectionTableUrlMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Protection Table Url Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

ProtectionTableUrlMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

ProtectionTableUrlMod.prototype.canUnload = function(compMgr) {
  return true;
}

ProtectionTableUrlMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new ProtectionTableUrl()).QueryInterface(iid);
  }
};

var UrlModInst = new ProtectionTableUrlMod();

function NSGetModule(compMgr, fileSpec) {
  return UrlModInst;
}
