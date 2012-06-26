// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gOCSPDialog = {
  _certDB         : null,
  _OCSPResponders : null,
  _cacheRadio     : 0,

  init: function ()
  {
    this._certDB = Components.classes["@mozilla.org/security/x509certdb;1"]
                             .getService(Components.interfaces.nsIX509CertDB);
    this._OCSPResponders = this._certDB.getOCSPResponders();

    var signingCA = document.getElementById("signingCA");
    const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
    for (var i = 0; i < this._OCSPResponders.length; ++i) {
      var ocspEntry = this._OCSPResponders.queryElementAt(i, nsIOCSPResponder);
      var menuitem = document.createElement("menuitem");
      menuitem.setAttribute("value", ocspEntry.responseSigner);
      menuitem.setAttribute("label", ocspEntry.responseSigner);
      signingCA.firstChild.appendChild(menuitem);
    }
    
    var signingCAPref = document.getElementById("security.OCSP.signingCA");
    if (!signingCAPref.hasUserValue)
      signingCA.selectedIndex = 0;
    else {
      // We need to initialize manually since auto-initialization is often 
      // called prior to menulist population above.
      signingCA.value = signingCAPref.value;
    }
    this.chooseServiceURL();
  },
  
  _updateUI: function (called_by)
  {
    var signingCA = document.getElementById("security.OCSP.signingCA");
    var serviceURL = document.getElementById("security.OCSP.URL");
    var securityOCSPEnabled = document.getElementById("security.OCSP.enabled");
    var requireWorkingOCSP = document.getElementById("security.OCSP.require");
    var enableOCSPBox = document.getElementById("enableOCSPBox");
    var certOCSP = document.getElementById("certOCSP");
    var proxyOCSP = document.getElementById("proxyOCSP");

    var OCSPPrefValue = parseInt(securityOCSPEnabled.value);

    if (called_by == 0) {
      // the radio button changed, or we init the stored value from prefs
      enableOCSPBox.checked = (OCSPPrefValue != 0);
    }
    else {
      // the user toggled the checkbox to enable/disable OCSP
      var new_val = 0;
      if (enableOCSPBox.checked) {
        // now enabled. if we have a cached radio val, restore it.
        // if not, use the first setting
        new_val = (this._cacheRadio > 0) ? this._cacheRadio : 1;
      }
      else {
        // now disabled. remember current value
        this._cacheRadio = OCSPPrefValue;
      }
      securityOCSPEnabled.value = OCSPPrefValue = new_val;
    }

    certOCSP.disabled = (OCSPPrefValue == 0);
    proxyOCSP.disabled = (OCSPPrefValue == 0);
    signingCA.disabled = serviceURL.disabled = OCSPPrefValue == 0 || OCSPPrefValue == 1;
    requireWorkingOCSP.disabled = (OCSPPrefValue == 0);
    
    return undefined;
  },
  
  chooseServiceURL: function ()
  {
    var signingCA = document.getElementById("signingCA");
    var serviceURL = document.getElementById("serviceURL");
    var CA = signingCA.value;
    
    const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
    for (var i = 0; i < this._OCSPResponders.length; ++i) {
      var ocspEntry = this._OCSPResponders.queryElementAt(i, nsIOCSPResponder);
      if (CA == ocspEntry.responseSigner) {
        serviceURL.value = ocspEntry.serviceURL;
        break;
      }
    }
  }
};
