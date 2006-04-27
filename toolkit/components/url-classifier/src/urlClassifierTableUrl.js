const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/lang.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/url-classifier/map.js",
  "chrome://global/content/url-classifier/url-canonicalizer.js"
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function UrlClassifierTableUrl() {
  G_Map.call(this);
  this.debugZone = "trtable-url";
}

UrlClassifierTableUrl.inherits(G_Map);

/**
 * Look up a URL in a URL table
 *
 * @returns Boolean true if the canonicalized url is in the table
 */
UrlClassifierTableUrl.prototype.find = function(url) {
  var canonicalized = PROT_URLCanonicalizer.canonicalizeURL_(url);
  // Uncomment for debugging
  G_Debug(this, "Looking up: " + url + " (" + canonicalized + ")");
  return this.find_(canonicalized);
}


// Module object
function UrlClassifierTableUrlMod() {
  this.firstTime = true;
  this.cid = Components.ID("{43399ee0-da0b-46a8-9541-08721265981c}");
  this.progid = "@mozilla.org/url-classifier/table;1?type=url";
}

UrlClassifierTableUrlMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier Table Url Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierTableUrlMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierTableUrlMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierTableUrlMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new UrlClassifierTableUrl()).QueryInterface(iid);
  }
};

var UrlModInst = new UrlClassifierTableUrlMod();

function NSGetModule(compMgr, fileSpec) {
  return UrlModInst;
}
