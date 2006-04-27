const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = true;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/arc4.js",
  "chrome://global/content/url-classifier/js/lang.js",
  "chrome://global/content/url-classifier/js/thread-queue.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // dep js/lang.js moz/prefs.js moz/filesystem.js
  "chrome://global/content/url-classifier/moz/alarm.js",
  "chrome://global/content/url-classifier/moz/base64.js",
  "chrome://global/content/url-classifier/moz/cryptohasher.js",
  "chrome://global/content/url-classifier/moz/lang.js",
  "chrome://global/content/url-classifier/moz/observer.js",
  "chrome://global/content/url-classifier/moz/protocol4.js",

  "chrome://global/content/url-classifier/application.js",
  "chrome://global/content/url-classifier/globalstore.js",
  "chrome://global/content/url-classifier/listmanager.js",
  "chrome://global/content/url-classifier/url-crypto.js",
  "chrome://global/content/url-classifier/url-crypto-key-manager.js", // dep url-crypto.js
  "chrome://global/content/url-classifier/wireformat.js",
  "chrome://global/content/url-classifier/xml-fetcher.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

// Module object
function UrlClassifierListManagerMod() {
  this.firstTime = true;
  this.cid = Components.ID("{ca168834-cc00-48f9-b83c-fd018e58cae3}");
  this.progid = "@mozilla.org/url-classifier/listmanager;1";
}

UrlClassifierListManagerMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier List Manager Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierListManagerMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierListManagerMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierListManagerMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new PROT_ListManager()).QueryInterface(iid);
  }
};

var ListManagerModInst = new UrlClassifierListManagerMod();

function NSGetModule(compMgr, fileSpec) {
  return ListManagerModInst;
}
