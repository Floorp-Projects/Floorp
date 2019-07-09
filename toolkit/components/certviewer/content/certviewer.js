/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

let gElements = {};

document.addEventListener("DOMContentLoaded", e => {
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
