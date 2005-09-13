# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gOCSPDialog = {
  _certDB         : null,
  _OCSPResponders : null,

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
  
  _updateUI: function ()
  {
    var signingCA = document.getElementById("security.OCSP.signingCA");
    var serviceURL = document.getElementById("security.OCSP.URL");
    var securityOCSPEnabled = document.getElementById("security.OCSP.enabled");

    var OCSPEnabled = parseInt(securityOCSPEnabled.value);
    signingCA.disabled = serviceURL.disabled = OCSPEnabled == 0 || OCSPEnabled == 1;
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
