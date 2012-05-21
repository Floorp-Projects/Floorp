# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

#include ./content/listmanager.js

var modScope = this;
function Init() {
  // Pull the library in.
  var jslib = Cc["@mozilla.org/url-classifier/jslib;1"]
              .getService().wrappedJSObject;
  Function.prototype.inherits = jslib.Function.prototype.inherits;
  modScope.G_Preferences = jslib.G_Preferences;
  modScope.G_PreferenceObserver = jslib.G_PreferenceObserver;
  modScope.G_ObserverServiceObserver = jslib.G_ObserverServiceObserver;
  modScope.G_Debug = jslib.G_Debug;
  modScope.G_Assert = jslib.G_Assert;
  modScope.G_debugService = jslib.G_debugService;
  modScope.G_Alarm = jslib.G_Alarm;
  modScope.BindToObject = jslib.BindToObject;
  modScope.PROT_XMLFetcher = jslib.PROT_XMLFetcher;
  modScope.PROT_UrlCryptoKeyManager = jslib.PROT_UrlCryptoKeyManager;
  modScope.RequestBackoff = jslib.RequestBackoff;

  // We only need to call Init once.
  modScope.Init = function() {};
}

function RegistrationData()
{
}
RegistrationData.prototype = {
    classID: Components.ID("{ca168834-cc00-48f9-b83c-fd018e58cae3}"),
    _xpcom_factory: {
        createInstance: function(outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            Init();
            return (new PROT_ListManager()).QueryInterface(iid);
        }
    },
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([RegistrationData]);
