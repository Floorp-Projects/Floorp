/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// NSS Dialog Service
// -----------------------------------------------------------------------

function dump(a) {
  Components.classes["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(a);
}

function NSSDialogs() { }

NSSDialogs.prototype = {
  classID: Components.ID("{cbc08081-49b6-4561-9c18-a7707a50bda1}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertificateDialogs, Ci.nsIClientAuthDialogs]),

  getString: function(aName) {
    if (!this.bundle) {
        this.bundle = Services.strings.createBundle("chrome://browser/locale/pippki.properties");
    }
    return this.bundle.GetStringFromName(aName);
  },

  formatString: function(aName, argList) {
    if (!this.bundle) {
      this.bundle = Services.strings.createBundle("chrome://browser/locale/pippki.properties");
    }
    return this.bundle.formatStringFromName(aName, argList, 1);
  },

  showPrompt: function(aTitle, aText, aButtons, aInputs) {
    let msg = {
      type: "Prompt:Show",
      title: aTitle,
      text: aText,
      buttons: aButtons,
      inputs: aInputs
    };
    let data = Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge).handleGeckoMessage(JSON.stringify(msg));
    return JSON.parse(data);
  },

  confirmDownloadCACert: function(aCtx, aCert, aTrust) {
    while (true) {
      let response = this.showPrompt(this.getString("downloadCert.title"),
                                     this.getString("downloadCert.message1"),
                                     [ this.getString("nssdialogs.ok.label"),
                                       this.getString("downloadCert.viewCert.label"),
                                       this.getString("nssdialogs.cancel.label")
                                     ],
                                     [ { type: "checkbox", id: "trustSSL", label: this.getString("downloadCert.trustSSL"), checked: false },
                                       { type: "checkbox", id: "trustEmail", label: this.getString("downloadCert.trustEmail"), checked: false },
                                       { type: "checkbox", id: "trustSign", label: this.getString("downloadCert.trustObjSign"), checked: false }
                                     ]);
      if (response.button == 1) {
        // they hit the "view cert" button, so show the cert and try again
        this.viewCert(aCtx, aCert);
        continue;
      } else if (response.button != 0) {
        return false;
      }

      aTrust.value = Ci.nsIX509CertDB.UNTRUSTED;
      if (response.trustSSL == "true") aTrust.value |= Ci.nsIX509CertDB.TRUSTED_SSL;
      if (response.trustEmail == "true") aTrust.value |= Ci.nsIX509CertDB.TRUSTED_EMAIL;
      if (response.trustSign == "true") aTrust.value |= Ci.nsIX509CertDB.TRUSTED_OBJSIGN;
      return true;
    }
  },

  notifyCACertExists: function(aCtx) {
    this.showPrompt(this.getString("caCertExists.title"), this.getString("caCertExists.message"), [], []);
  },

  setPKCS12FilePassword: function(aCtx, aPassword) {
    // this dialog is never shown in Fennec; in Desktop it is shown while backing up a personal
    // certificate to a file via Preferences->Advanced->Encryption->View Certificates->Your Certificates
    throw "Unimplemented";
  },

  getPKCS12FilePassword: function(aCtx, aPassword) {
    let response = this.showPrompt(this.getString("pkcs12.getpassword.title"),
                                   this.getString("pkcs12.getpassword.message"),
                                   [ this.getString("nssdialogs.ok.label"),
                                     this.getString("nssdialogs.cancel.label")
                                   ],
                                   [ { type: "password", id: "pw" } ]);
    if (response.button != 0) {
      return false;
    }
    aPassword.value = response.pw;
    return true;
  },

  certInfoSection: function(aHeading, aDataPairs, aTrailingNewline = true) {
    var str = "<big>" + this.getString(aHeading) + "</big><br/>";
    for (var i = 0; i < aDataPairs.length; i += 2) {
      str += this.getString(aDataPairs[i]) + ": " + aDataPairs[i+1] + "<br/>";
    }
    return str + (aTrailingNewline ? "<br/>" : "");
  },

  viewCert: function(aCtx, aCert) {
    this.showPrompt(this.getString("certmgr.title"),
                    "",
                    [ this.getString("nssdialogs.ok.label") ],
                    [ { type: "label", label:
                        this.certInfoSection("certmgr.subjectinfo.label",
                          ["certmgr.certdetail.cn", aCert.commonName,
                           "certmgr.certdetail.o", aCert.organization,
                           "certmgr.certdetail.ou", aCert.organizationalUnit,
                           "certmgr.certdetail.serialnumber", aCert.serialNumber]) +
                        this.certInfoSection("certmgr.issuerinfo.label",
                          ["certmgr.certdetail.cn", aCert.issuerCommonName,
                           "certmgr.certdetail.o", aCert.issuerOrganization,
                           "certmgr.certdetail.ou", aCert.issuerOrganizationUnit]) +
                        this.certInfoSection("certmgr.validity.label",
                          ["certmgr.issued", aCert.validity.notBeforeLocalDay,
                           "certmgr.expires", aCert.validity.notAfterLocalDay]) +
                        this.certInfoSection("certmgr.fingerprints.label",
                          ["certmgr.certdetail.sha1fingerprint", aCert.sha1Fingerprint,
                           "certmgr.certdetail.md5fingerprint", aCert.md5Fingerprint], false) }
                    ]);
  },

  crlImportStatusDialog: function(aCtx, aCrl) {
    // this dialog is never shown in Fennec; in Desktop it is shown after importing a CRL
    // via Preferences->Advanced->Encryption->Revocation Lists->Import.
    throw "Unimplemented";
  },

  viewCertDetails: function(details) {
    this.showPrompt(this.getString("clientAuthAsk.message3"),
                    '',
                    [ this.getString("nssdialogs.ok.label") ],
                    [ { type: "label", label: details
                      }
                    ]);
  },

  ChooseCertificate: function(aCtx, cn, organization, issuer, certNickList, certDetailsList, count, selectedIndex, canceled) {
    let rememberSetting = true;
    var pref = Cc['@mozilla.org/preferences-service;1']
               .getService(Components.interfaces.nsIPrefService);
    if (pref) {
      pref = pref.getBranch(null);
      try {
        rememberSetting = pref.getBoolPref("security.remember_cert_checkbox_default_setting");
      } catch (e) {
        // pref is missing
      }
    }

    let organizationString = this.formatString("clientAuthAsk.organization",
                                               [organization]);
    let issuerString = this.formatString("clientAuthAsk.issuer",
                                         [issuer]);
    let serverRequestedDetails = cn + '<br/>' + organizationString + '<br/>' + issuerString;

    selectedIndex = 0;
    while (true) {
      let response = this.showPrompt(this.getString("clientAuthAsk.title"),
                                     this.getString("clientAuthAsk.message1"),
                                     [ this.getString("nssdialogs.ok.label"),
                                       this.getString("clientAuthAsk.viewCert.label"),
                                       this.getString("nssdialogs.cancel.label")
                                     ],
                                     [ { type: "label", id: "requestedDetails", label: serverRequestedDetails },
                                       { type: "menulist", id: "nicknames", label: this.getString("clientAuthAsk.message2"), values: certNickList, selected: selectedIndex },
                                       { type: "checkbox", id: "rememberBox", label: this.getString("clientAuthAsk.remember.label"), checked: rememberSetting },
                                     ]);
      selectedIndex = response.nicknames;
      if (response.button == 1) {
        this.viewCertDetails(certDetailsList[selectedIndex]);
        continue;
      } else if (response.button == 0) {
        canceled.value = false;
        if (response.rememberBox == "true") {
          aCtx.QueryInterface(Ci.nsIClientAuthUserDecision).rememberClientAuthCertificate = true;
        }
        return true;
      }
      canceled.value = true;
      return false;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NSSDialogs]);
