const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = false;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://global/content/url-classifier/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  "chrome://global/content/url-classifier/js/arc4.js",
  "chrome://global/content/url-classifier/js/lang.js",

  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/url-classifier/map.js",
  "chrome://global/content/url-classifier/moz/base64.js",
  "chrome://global/content/url-classifier/moz/cryptohasher.js",
  "chrome://global/content/url-classifier/enchash-decrypter.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

function UrlClassifierTableEnchash() {
  G_Map.call(this);
  this.debugZone = "trtable-enchash";
  this.enchashDecrypter_ = new PROT_EnchashDecrypter();
}

UrlClassifierTableEnchash.inherits(G_Map);

/**
 * Look up a URL in an enchashDB
 *
 * @returns Boolean indicating whether the URL matches the regular
 *                  expression contained in the table value
 */
UrlClassifierTableEnchash.prototype.exists = function(url) {
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
function UrlClassifierTableEnchashMod() {
  this.firstTime = true;
  this.cid = Components.ID("{04f15d1d-2db8-4b8e-91d7-82f30308b434}");
  this.progid = "@mozilla.org/url-classifier/table;1?type=enchash";
}

UrlClassifierTableEnchashMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "UrlClassifier Table Enchash Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

UrlClassifierTableEnchashMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

UrlClassifierTableEnchashMod.prototype.canUnload = function(compMgr) {
  return true;
}

UrlClassifierTableEnchashMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new UrlClassifierTableEnchash()).QueryInterface(iid);
  }
};

var EnchashModInst = new UrlClassifierTableEnchashMod();

function NSGetModule(compMgr, fileSpec) {
  return EnchashModInst;
}
