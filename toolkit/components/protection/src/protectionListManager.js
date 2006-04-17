const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = true;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/protection/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/protection/js/arc4.js",
  "chrome://global/content/protection/js/lang.js",
  "chrome://global/content/protection/js/thread-queue.js",

  "chrome://global/content/protection/moz/preferences.js",
  "chrome://global/content/protection/moz/filesystem.js",
  "chrome://global/content/protection/moz/debug.js", // dep js/lang.js moz/prefs.js moz/filesystem.js
  "chrome://global/content/protection/moz/alarm.js",
  "chrome://global/content/protection/moz/base64.js",
  "chrome://global/content/protection/moz/cryptohasher.js",
  "chrome://global/content/protection/moz/lang.js",
  "chrome://global/content/protection/moz/observer.js",
  "chrome://global/content/protection/moz/protocol4.js",

  "chrome://global/content/protection/application.js",
  "chrome://global/content/protection/globalstore.js",
  "chrome://global/content/protection/listmanager.js",
  "chrome://global/content/protection/url-crypto.js",
  "chrome://global/content/protection/url-crypto-key-manager.js", // dep url-crypto.js
  "chrome://global/content/protection/wireformat.js",
  "chrome://global/content/protection/xml-fetcher.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

// Module object
function ProtectionListManagerMod() {
  this.firstTime = true;
  this.cid = Components.ID("{ca168834-cc00-48f9-b83c-fd018e58cae3}");
  this.progid = "@mozilla.org/protection/protectionlistmanager;1";
}

ProtectionListManagerMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Protection List Manager Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

ProtectionListManagerMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

ProtectionListManagerMod.prototype.canUnload = function(compMgr) {
  return true;
}

ProtectionListManagerMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new PROT_ListManager()).QueryInterface(iid);
  }
};

var ListManagerModInst = new ProtectionListManagerMod();

function NSGetModule(compMgr, fileSpec) {
  return ListManagerModInst;
}
