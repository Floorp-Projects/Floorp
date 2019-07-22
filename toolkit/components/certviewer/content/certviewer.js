/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

import { parse } from "./certDecoder.js";
import { pemToDER } from "./utils.js";
let gElements = {};

document.addEventListener("DOMContentLoaded", async e => {
  gElements.certificateSection = document.querySelector("certificate-section");
  let url = new URL(document.URL);
  let certInfo = url.searchParams.getAll("cert");
  if (certInfo.length === 0) {
    await render(true);
    return;
  }
  certInfo = certInfo.map(cert => decodeURIComponent(cert));
  await buildChain(certInfo);
});

export const updateSelectedItem = (() => {
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

const render = async error => {
  await customElements.whenDefined("certificate-section");
  const CertificateSection = customElements.get("certificate-section");
  document.querySelector("body").append(new CertificateSection(error));
  return Promise.resolve();
};

const buildChain = async chain => {
  await Promise.all(
    chain
      .map(cert => {
        try {
          return pemToDER(cert);
        } catch (err) {
          return Promise.reject(err);
        }
      })
      .map(cert => {
        try {
          return parse(cert);
        } catch (err) {
          return Promise.reject(err);
        }
      })
  )
    .then(certs => {
      if (certs.length === 0) {
        return Promise.reject();
      }
      return render(false);
    })
    .catch(err => {
      render(true);
    });
};
