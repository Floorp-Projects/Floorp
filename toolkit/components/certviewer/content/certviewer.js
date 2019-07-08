/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

let gElements = {};

document.addEventListener("DOMContentLoaded", e => {
  RPMAddMessageListener("Certificate", showCertificate);
  RPMSendAsyncMessage("getCertificate");
  gElements.certificateSection = document.querySelector("certificate-section");
});

const updateSelectedItem = (() => {
  let state;
  return selectedItem => {
    if (selectedItem) {
      if (state !== selectedItem) {
        state = selectedItem;
        gElements.certificateSection.updateCertificateSource(selectedItem);
        gElements.certificateSection.updateSelectedTab(selectedItem);
      }
    }
    return state;
  };
})();

const str2ab = str => {
  let buf = new ArrayBuffer(str.length);
  let bufView = new Uint8Array(buf);
  for (let i = 0; i < str.length; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return buf;
};

const showCertificate = message => {
  console.log("certificate ", message.data.certs);
  let chain = message.data.certs;
  let builtChain = chain.map(cert => {
    return str2ab(cert);
  });
  console.log("builtChain ", builtChain);
};
