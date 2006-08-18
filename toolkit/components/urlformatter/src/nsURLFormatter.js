/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the nsURLFormatterService.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht <axel@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dietrich Ayala <dietrich@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * @class nsURLFormatterService
 *
 * nsURLFormatterService exposes methods to substitute variables in URL formats.
 *
 * Mozilla Applications linking to Mozilla websites are strongly encouraged to use
 * URLs of the following format:
 *
 *   http[s]://%LOCALE%.%SERVICE%.mozilla.[com|org]/%LOCALE%/
 */

/**
 * constants
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const CID =         Components.ID("{e6156350-2be8-11db-a98b-0800200c9a66}");
const CONTRACT_ID = "@mozilla.org/browser/URLFormatterService;1";
const CLASS_NAME =  "Browser URL Formatter Service";

const APPINFO =     Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);

/**
 * class definition
 */

function nsURLFormatterService() {
}

nsURLFormatterService.prototype = {

  /**
   * Built-in values
   */
  _defaults: {
    LOCALE: function() {
      var chromereg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                      getService(Ci.nsIXULChromeRegistry);
      return chromereg.getSelectedLocale('global');
    },
    VENDOR:           function() { return APPINFO.vendor; },
    NAME:             function() { return APPINFO.name; },
    ID:               function() { return APPINFO.ID; },
    VERSION:          function() { return APPINFO.version; },
    APPBUILDID:       function() { return APPINFO.appBuildID; },
    PLATFORMVERSION:  function() { return APPINFO.platformVersion; },
    PLATFORMBUILDID:  function() { return APPINFO.platformBuildID; },
    APP:              function() { return APPINFO.name.toLowerCase(); }
  },

  /**
   *
   */
  formatURL: function uf_formatURL(aFormat, aVars) {
    var repl = [];
    if (aVars) {
      var keys = aVars.getKeys({});
      for (var i = 0; i < keys.length; i++) {
        var val = XPCNativeWrapper(aVars.getValue(keys[i]).QueryInterface(Ci.nsISupportsString));
        repl[keys[i]] = val;
      }
    }
      
    var _this = this;
    var replacer = function(aMatch, aKey) {
      if (repl[aKey]) // handle vars from caller
        return repl[aKey];
      if (_this._defaults[aKey]) // supported defaults
        return _this._defaults[aKey]();
      dump(aKey + " not found");
      return '';
    }
    return aFormat.replace(/%([A-Z]+)%/gi, replacer);
  },

  /**
   *
   */
  formatURLPref: function uf_formatURLPref(aPref, aVars) {
    var format = null;
    var PS = Cc['@mozilla.org/preferences-service;1'].
             getService(Ci.nsIPrefBranch);
    try {
      format = PS.getComplexValue(aPref, Ci.nsIPrefLocalizedString).data;
    } catch(ex) { dump(ex + "\n"); }
    if (!format) {
      format = PS.getComplexValue(aPref, Ci.nsISupportsString).data;
    }
    if (!format) {
      dump(aPref + " not found");
      return 'about:blank';
    }
    return this.formatURL(format, aVars);
  },

  QueryInterface: function uf_QueryInterface(aIID) {
    if (!aIID.equals(Ci.nsIURLFormatter) &&    
        !aIID.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * XPCOM Registration, etc
 */
var nsURLFormatterFactory = {
  createInstance: function (aOuter, aIID) {
    if (aOuter != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return (new nsURLFormatterService()).QueryInterface(aIID);
  }
};

var nsURLFormatterModule = {
  _firstTime: true,
  registerSelf: function(aCompMgr, aFileSpec, aLocation, aType) {
    aCompMgr = aCompMgr.QueryInterface(Ci.nsIComponentRegistrar);
    aCompMgr.registerFactoryLocation(CID, CLASS_NAME, 
        CONTRACT_ID, aFileSpec, aLocation, aType);
  },

  unregisterSelf: function(aCompMgr, aLocation, aType) {
    aCompMgr = aCompMgr.QueryInterface(Cinterfaces.nsIComponentRegistrar);
    aCompMgr.unregisterFactoryLocation(CID, aLocation);        
  },
  
  getClassObject: function(aCompMgr, aCID, aIID) {
    if (!aIID.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    if (aCID.equals(CID))
      return nsURLFormatterFactory;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  canUnload: function(aCompMgr) { return true; }
};

function NSGetModule(aCompMgr, aFileSpec) { return nsURLFormatterModule; }
