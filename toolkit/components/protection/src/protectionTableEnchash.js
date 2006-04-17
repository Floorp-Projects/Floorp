const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/protection/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/protection/js/arc4.js",
  "chrome://global/content/protection/js/lang.js",

  "chrome://global/content/protection/moz/preferences.js",
  "chrome://global/content/protection/moz/filesystem.js",
  "chrome://global/content/protection/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/protection/map.js",
  "chrome://global/content/protection/moz/base64.js",
  "chrome://global/content/protection/moz/cryptohasher.js",
  "chrome://global/content/protection/enchash-decrypter.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function ProtectionTableEnchash() {
  G_Map.call(this);
  this.debugZone = "trtable-enchash";
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();
}

ProtectionTableEnchash.inherits(G_Map);

/**
 * Look up a URL in an enchashDB
 *
 * @returns Boolean indicating whether the URL matches the regular
 *                  expression contained in the table value
 */
ProtectionTableEnchash.prototype.find = function(url) {
  var host = this.enchashDecrypter_.getCanonicalHost(url);

  for (var i = 0; i < PROT_EnchashDecrypter.MAX_DOTS + 1; i++) {
    var key = this.enchashDecrypter_.getLookupKey(host);

    var encrypted = this.find_(key);
    if (encrypted) {
      G_Debug(this, "Enchash DB has host " + host);

      // We have encrypted regular expressions for this host. Let's 
      // decrypt them and see if we have a match.
      var decrypted = this.enchashDecrypter_.decryptData(encrypted, host);
      var res = this.enchashDecrypter_.parseRegExps(decrypted);
      for (var j = 0; j < res.length; j++) {
        if (res[j].test(url))
          return true;
      }
    }

    var index = host.indexOf(".");
    if (index == -1)
      break;
    host = host.substring(index + 1);
  }
  return false;
}


// Module object
function ProtectionTableEnchashMod() {
  this.firstTime = true;
  this.cid = Components.ID("{04f15d1d-2db8-4b8e-91d7-82f30308b434}");
  this.progid = "@mozilla.org/protection/protectiontable;1?type=enchash";
}

ProtectionTableEnchashMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Protection Table Enchash Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

ProtectionTableEnchashMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

ProtectionTableEnchashMod.prototype.canUnload = function(compMgr) {
  return true;
}

ProtectionTableEnchashMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new ProtectionTableEnchash()).QueryInterface(iid);
  }
};

var EnchashModInst = new ProtectionTableEnchashMod();

function NSGetModule(compMgr, fileSpec) {
  return EnchashModInst;
}
