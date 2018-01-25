/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

// -----------------------------------------------------------------------
// NSS Dialog Service
// -----------------------------------------------------------------------

function NSSDialogs() { }

NSSDialogs.prototype = {
  classID: Components.ID("{cbc08081-49b6-4561-9c18-a7707a50bda1}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICertificateDialogs, Ci.nsIClientAuthDialogs]),

  /**
   * Escapes the given input via HTML entity encoding. Used to prevent HTML
   * injection when the input is to be placed inside an HTML body, but not in
   * any other context.
   *
   * @param {String} input The input to interpret as a plain string.
   * @returns {String} The escaped input.
   */
  escapeHTML: function(input) {
    return input.replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#x27;")
                .replace(/\//g, "&#x2F;");
  },

  getString: function(aName) {
    if (!this.bundle) {
        this.bundle = Services.strings.createBundle("chrome://browser/locale/pippki.properties");
    }
    return this.bundle.GetStringFromName(aName);
  },

  formatString: function(aName, argList) {
    if (!this.bundle) {
      this.bundle =
        Services.strings.createBundle("chrome://browser/locale/pippki.properties");
    }
    let escapedArgList = Array.from(argList, x => this.escapeHTML(x));
    return this.bundle.formatStringFromName(aName, escapedArgList,
                                            escapedArgList.length);
  },

  getPrompt: function(aTitle, aText, aButtons, aCtx) {
    let win = null;
    try {
      win = aCtx.getInterface(Ci.nsIDOMWindow);
    } catch (e) {
    }
    return new Prompt({
      window: win,
      title: aTitle,
      text: aText,
      buttons: aButtons,
    });
  },

  showPrompt: function(aPrompt) {
    let response = null;
    aPrompt.show(function(data) {
      response = data;
    });

    // Spin this thread while we wait for a result
    Services.tm.spinEventLoopUntil(() => response != null);

    return response;
  },

  confirmDownloadCACert: function(aCtx, aCert, aTrust) {
    while (true) {
      let prompt = this.getPrompt(this.getString("downloadCert.title"),
                                  this.getString("downloadCert.message1"),
                                  [ this.getString("nssdialogs.ok.label"),
                                    this.getString("downloadCert.viewCert.label"),
                                    this.getString("nssdialogs.cancel.label")
                                  ], aCtx);

      prompt.addCheckbox({ id: "trustSSL", label: this.getString("downloadCert.trustSSL"), checked: false })
            .addCheckbox({ id: "trustEmail", label: this.getString("downloadCert.trustEmail"), checked: false });
      let response = this.showPrompt(prompt);

      // they hit the "view cert" button, so show the cert and try again
      if (response.button == 1) {
        this.viewCert(aCtx, aCert);
        continue;
      } else if (response.button != 0) {
        return false;
      }

      aTrust.value = Ci.nsIX509CertDB.UNTRUSTED;
      if (response.trustSSL) aTrust.value |= Ci.nsIX509CertDB.TRUSTED_SSL;
      if (response.trustEmail) aTrust.value |= Ci.nsIX509CertDB.TRUSTED_EMAIL;
      return true;
    }
  },

  setPKCS12FilePassword: function(aCtx, aPassword) {
    // this dialog is never shown in Fennec; in Desktop it is shown while backing up a personal
    // certificate to a file via Preferences->Advanced->Encryption->View Certificates->Your Certificates
    throw "Unimplemented";
  },

  getPKCS12FilePassword: function(aCtx, aPassword) {
    let prompt = this.getPrompt(this.getString("pkcs12.getpassword.title"),
                                this.getString("pkcs12.getpassword.message"),
                                [ this.getString("nssdialogs.ok.label"),
                                  this.getString("nssdialogs.cancel.label")
                                ], aCtx).addPassword({id: "pw"});
    let response = this.showPrompt(prompt);
    if (response.button != 0) {
      return false;
    }

    aPassword.value = response.pw;
    return true;
  },

  certInfoSection: function(aHeading, aDataPairs, aTrailingNewline = true) {
    let certInfoStrings = [
      "<big>" + this.getString(aHeading) + "</big>",
    ];

    for (let i = 0; i < aDataPairs.length; i += 2) {
      let key = aDataPairs[i];
      let value = aDataPairs[i + 1];
      certInfoStrings.push(this.formatString(key, [value]));
    }

    if (aTrailingNewline) {
      certInfoStrings.push("<br/>");
    }

    return certInfoStrings.join("<br/>");
  },

  viewCert: function(aCtx, aCert) {
    let p = this.getPrompt(this.getString("certmgr.title"), "", [
                             this.getString("nssdialogs.ok.label"),
                           ], aCtx);
    p.addLabel({ label: this.certInfoSection("certmgr.subjectinfo.label",
                          ["certdetail.cn", aCert.commonName,
                           "certdetail.o", aCert.organization,
                           "certdetail.ou", aCert.organizationalUnit,
                           "certdetail.serialnumber", aCert.serialNumber])})
     .addLabel({ label: this.certInfoSection("certmgr.issuerinfo.label",
                          ["certdetail.cn", aCert.issuerCommonName,
                           "certdetail.o", aCert.issuerOrganization,
                           "certdetail.ou", aCert.issuerOrganizationUnit])})
     .addLabel({ label: this.certInfoSection("certmgr.periodofvalidity.label",
                          ["certdetail.notBefore", aCert.validity.notBeforeLocalDay,
                           "certdetail.notAfter", aCert.validity.notAfterLocalDay])})
     .addLabel({ label: this.certInfoSection("certmgr.fingerprints.label",
                          ["certdetail.sha256fingerprint", aCert.sha256Fingerprint,
                           "certdetail.sha1fingerprint", aCert.sha1Fingerprint],
                          false) });
    this.showPrompt(p);
  },

  /**
   * Returns a list of details of the given cert relevant for TLS client
   * authentication.
   *
   * @param {nsIX509Cert} cert Cert to get the details of.
   * @returns {String} <br/> delimited list of details.
   */
  getCertDetails: function(cert) {
    let detailLines = [
      this.formatString("clientAuthAsk.issuedTo", [cert.subjectName]),
      this.formatString("clientAuthAsk.serial", [cert.serialNumber]),
      this.formatString("clientAuthAsk.validityPeriod",
                        [cert.validity.notBeforeLocalTime,
                         cert.validity.notAfterLocalTime]),
    ];
    let keyUsages = cert.keyUsages;
    if (keyUsages) {
      detailLines.push(this.formatString("clientAuthAsk.keyUsages",
                                         [keyUsages]));
    }
    let emailAddresses = cert.getEmailAddresses({});
    if (emailAddresses.length > 0) {
      let joinedAddresses = emailAddresses.join(", ");
      detailLines.push(this.formatString("clientAuthAsk.emailAddresses",
                                         [joinedAddresses]));
    }
    detailLines.push(this.formatString("clientAuthAsk.issuedBy",
                                       [cert.issuerName]));
    detailLines.push(this.formatString("clientAuthAsk.storedOn",
                                       [cert.tokenName]));

    return detailLines.join("<br/>");
  },

  viewCertDetails: function(details, ctx) {
    let p = this.getPrompt(this.getString("clientAuthAsk.message3"),
                    "",
                    [ this.getString("nssdialogs.ok.label") ], ctx);
    p.addLabel({ label: details });
    this.showPrompt(p);
  },

  chooseCertificate: function(ctx, hostname, port, organization, issuerOrg,
                              certList, selectedIndex) {
    let rememberSetting =
      Services.prefs.getBoolPref("security.remember_cert_checkbox_default_setting");

    let serverRequestedDetails = [
      this.formatString("clientAuthAsk.hostnameAndPort",
                        [hostname, port.toString()]),
      this.formatString("clientAuthAsk.organization", [organization]),
      this.formatString("clientAuthAsk.issuer", [issuerOrg]),
    ].join("<br/>");

    let certNickList = [];
    let certDetailsList = [];
    for (let i = 0; i < certList.length; i++) {
      let cert = certList.queryElementAt(i, Ci.nsIX509Cert);
      certNickList.push(this.formatString("clientAuthAsk.nickAndSerial",
                                          [cert.displayName, cert.serialNumber]));
      certDetailsList.push(this.getCertDetails(cert));
    }

    selectedIndex.value = 0;
    while (true) {
      let buttons = [
        this.getString("nssdialogs.ok.label"),
        this.getString("clientAuthAsk.viewCert.label"),
        this.getString("nssdialogs.cancel.label"),
      ];
      let prompt = this.getPrompt(this.getString("clientAuthAsk.title"),
                                  this.getString("clientAuthAsk.message1"),
                                  buttons, ctx)
      .addLabel({ id: "requestedDetails", label: serverRequestedDetails } )
      .addMenulist({
        id: "nicknames",
        label: this.getString("clientAuthAsk.message2"),
        values: certNickList,
        selected: selectedIndex.value,
      }).addCheckbox({
        id: "rememberBox",
        label: this.getString("clientAuthAsk.remember.label"),
        checked: rememberSetting
      });
      let response = this.showPrompt(prompt);
      selectedIndex.value = response.nicknames;
      if (response.button == 1 /* buttons[1] */) {
        this.viewCertDetails(certDetailsList[selectedIndex.value], ctx);
        continue;
      } else if (response.button == 0 /* buttons[0] */) {
        if (response.rememberBox == true) {
          let caud = ctx.QueryInterface(Ci.nsIClientAuthUserDecision);
          if (caud) {
            caud.rememberClientAuthCertificate = true;
          }
        }
        return true;
      }
      return false;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([NSSDialogs]);
