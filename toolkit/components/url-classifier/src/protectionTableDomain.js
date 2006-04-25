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

function ProtectionTableDomain() {
  G_Map.call(this);
  this.debugZone = "trtable-domain";
}

ProtectionTableDomain.inherits(G_Map);

/**
 * Look up a URL in a domain table
 *
 * @returns Boolean true if the url domain is in the table
 */
ProtectionTableDomain.prototype.find = function(url) {
  var urlObj = Cc["@mozilla.org/network/standard-url;1"]
               .createInstance(Ci.nsIURL);
  urlObj.spec = url;
  var host = urlObj.host;
  var components = host.split(".");

  // We don't have a good way map from hosts to domains, so we instead try
  // each possibility. Could probably optimize to start at the second dot?
  for (var i = 0; i < components.length - 1; i++) {
    host = components.slice(i).join(".");
    var val = this.find_(host);
    if (val)
      return true;
  }
  return false;
}


// Module object
function ProtectionTableDomainMod() {
  this.firstTime = true;
  this.cid = Components.ID("{3b5004c6-3fcd-4b12-b311-a4dfbeaf27aa}");
  this.progid = "@mozilla.org/protection/protectiontable;1?type=domain";
}

ProtectionTableDomainMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Protection Table Domain Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

ProtectionTableDomainMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

ProtectionTableDomainMod.prototype.canUnload = function(compMgr) {
  return true;
}

ProtectionTableDomainMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new ProtectionTableDomain()).QueryInterface(iid);
  }
};

var DomainModInst = new ProtectionTableDomainMod();

function NSGetModule(compMgr, fileSpec) {
  return DomainModInst;
}
