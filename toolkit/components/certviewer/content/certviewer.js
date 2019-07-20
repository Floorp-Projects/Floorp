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
  let certInfo = url.searchParams.get("cert");
  if (!certInfo) {
    await render(true);
    return;
  }
  await buildChain(decodeURIComponent(certInfo));
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

const buildChain = async certBase64 => {
  let certDER;
  try {
    certDER = pemToDER(certBase64);
  } catch (err) {
    await render(true);
    return;
  }

  if (certDER == null) {
    await render(true);
    return;
  }

  try {
    await parse(certDER);
  } catch (err) {
    await render(true);
    return;
  }

  await render(false);
};
