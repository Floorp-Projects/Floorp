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

const createEntryItem = (label, info) => {
  return {
    label,
    info,
  };
};

const adjustCertInformation = cert => {
  let certItems = [];

  // Subject Name section
  certItems.push({
    sectionTitle: "Subject Name",
    sectionItems: cert.subject.entries.map(entry =>
      createEntryItem(entry[0], entry[1])
    ),
    Critical: false,
  });

  // Issuer Name section
  certItems.push({
    sectionTitle: "Issuer Name",
    sectionItems: cert.issuer.entries.map(entry =>
      createEntryItem(entry[0], entry[1])
    ),
    Critical: false,
  });

  // Validity section
  certItems.push({
    sectionTitle: "Validity",
    sectionItems: [
      createEntryItem("Not Before", cert.notBefore),
      createEntryItem("Not After", cert.notAfter),
    ],
    Critical: false,
  });

  // Subject Alt Names section
  certItems.push({
    sectionTitle: "Subject Alt Names",
    sectionItems: cert.ext.san.altNames.map(entry =>
      createEntryItem(entry[0], entry[1])
    ),
    Critical: cert.ext.san.critical || false,
  });

  // Public Key Info section
  certItems.push({
    sectionTitle: "Public Key Info",
    sectionItems: [
      createEntryItem("Algorithm", cert.subjectPublicKeyInfo.kty),
      createEntryItem("Key size", cert.subjectPublicKeyInfo.keysize),
      createEntryItem("Curve", cert.subjectPublicKeyInfo.crv),
      createEntryItem("Public Value", cert.subjectPublicKeyInfo.xy),
      createEntryItem("Exponent", cert.subjectPublicKeyInfo.e),
      createEntryItem("Modulus", cert.subjectPublicKeyInfo.n),
    ],
    Critical: false,
  });

  // Miscellaneous section
  certItems.push({
    sectionTitle: "Miscellaneous",
    sectionItems: [
      createEntryItem("Serial Number", cert.serialNumber),
      createEntryItem("Signature Algorithm", cert.signature.name),
      createEntryItem("Version", cert.version),
      createEntryItem("Download", cert.files.pem),
    ],
    Critical: false,
  });

  // Fingerprints section
  certItems.push({
    sectionTitle: "Fingerprints",
    sectionItems: [
      createEntryItem("SHA-256", cert.fingerprint.sha256),
      createEntryItem("SHA-1", cert.fingerprint.sha1),
    ],
    Critical: false,
  });

  // Basic Constraints section
  certItems.push({
    sectionTitle: "Basic Constraints",
    sectionItems: [
      createEntryItem("Certificate Authority", cert.ext.basicConstraintscA),
    ],
    Critical: cert.ext.basicConstraints.critical || false,
  });

  // Key Usages section
  certItems.push({
    sectionTitle: "Key Usages",
    sectionItems: [createEntryItem("Purposes", cert.ext.keyUsages.purposes)],
    Critical: cert.ext.keyUsages.critical || false,
  });

  // Extended Key Usages section
  certItems.push({
    sectionTitle: "Extended Key Usages",
    sectionItems: [createEntryItem("Purposes", cert.ext.eKeyUsages.purposes)],
    Critical: cert.ext.eKeyUsages.critical || false,
  });

  // OCSP Stapling section
  certItems.push({
    sectionTitle: "OCSP Stapling",
    sectionItems: [createEntryItem("Required", cert.ext.ocspStaple.critical)],
    Critical: cert.ext.ocspStaple.critical || false,
  });

  // Subject Key ID section
  certItems.push({
    sectionTitle: "Subject Key ID",
    sectionItems: [createEntryItem("Key ID", cert.ext.sKID.id)],
    Critical: cert.ext.sKID.critical || false,
  });

  // Authority Key ID section
  certItems.push({
    sectionTitle: "Authority Key ID",
    sectionItems: [createEntryItem("Key ID", cert.ext.aKID.id)],
    Critical: cert.ext.aKID.critical || false,
  });

  // CRL Endpoints section
  certItems.push({
    sectionTitle: "CRL Endpoints",
    sectionItems: cert.ext.crlPoints.points.map(entry => {
      let label = "Distribution Point";
      return createEntryItem(label, entry);
    }),
    Critical: cert.ext.crlPoints.critical || false,
  });

  // // Authority Info (AIA) section
  let items = [];
  cert.ext.aia.descriptions.forEach(entry => {
    items.push(createEntryItem("Location", entry.location));
    items.push(createEntryItem("Method", entry.method));
  });
  certItems.push({
    sectionTitle: "Authority Info (AIA)",
    sectionItems: items,
    Critical: cert.ext.aia.critical || false,
  });

  // Certificate Policies section
  items = [];
  cert.ext.cp.policies.forEach(entry => {
    items.push(createEntryItem("Policy", entry.name + " ( " + entry.id + " )"));
    items.push(createEntryItem("Value", entry.value));
    if (entry.qualifiers) {
      entry.qualifiers.forEach(qualifier => {
        items.push(
          createEntryItem(
            "Qualifier",
            qualifier.name + " ( " + qualifier.id + " )"
          )
        );
        items.push(createEntryItem("Value", qualifier.value));
      });
    }
  });
  certItems.push({
    sectionTitle: "Certificate Policies",
    sectionItems: items,
    Critical: cert.ext.cp.critical || false,
  });

  // Embedded SCTs section
  items = [];
  cert.ext.scts.timestamps.forEach(entry => {
    for (let key of Object.keys(entry)) {
      items.push(createEntryItem(key, entry[key]));
    }
  });
  certItems.push({
    sectionTitle: "Embedded SCTs",
    sectionItems: items,
    Critical: cert.ext.scts.critical || false,
  });

  certItems.push({
    tabName: cert.subject.cn,
  });

  return certItems;
};

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

  let cert;
  try {
    cert = await parse(certDER);
  } catch (err) {
    await render(true);
    return;
  }
  // Depends on https://bugzilla.mozilla.org/show_bug.cgi?id=1567561
  let adjustedCerts = adjustCertInformation(cert);
  console.log(adjustedCerts); // to avoid lint error
  await render(false);
};
