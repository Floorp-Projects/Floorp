/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutCertViewerHandler"];

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);

const TYPE_UNKNOWN = 0;
const TYPE_CA = 1;
const TYPE_USER = 2;
const TYPE_EMAIL = 4;
const TYPE_SERVER = 8;
const TYPE_ANY = 0xffff;

var AboutCertViewerHandler = {
  _inited: false,
  _topics: ["getCertificates"],

  initCerts() {
    let certs = {
      [TYPE_UNKNOWN]: [],
      [TYPE_CA]: [],
      [TYPE_USER]: [],
      [TYPE_EMAIL]: [],
      [TYPE_SERVER]: [],
      [TYPE_ANY]: [],
    };
    let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
      Ci.nsIX509CertDB
    );
    let certcache = certdb.getCerts();
    for (let cert of certcache) {
      for (let certType of Object.keys(certs)) {
        if (certType & cert.certType) {
          certs[cert.certType].push({
            displayName: cert.displayName,
            derb64: cert.getBase64DERString(),
          });
        }
      }
    }
    return certs;
  },

  init() {
    this.pageListener = new RemotePages("about:certificate");
    this.receiveMessage = this.receiveMessage.bind(this);
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this.receiveMessage);
    }
    this._inited = true;
  },

  uninit() {
    if (!this._inited) {
      return;
    }
    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this.receiveMessage);
    }
    this.pageListener.destroy();
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "getCertificates": {
        let certs = this.initCerts();
        aMessage.target.sendAsyncMessage("certificates", {
          certs,
        });
        break;
      }
    }
  },
};
