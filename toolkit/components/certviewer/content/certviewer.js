/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

import { parse } from "./certDecoder.js";
import { pemToDER } from "./utils.js";

document.addEventListener("DOMContentLoaded", async e => {
  let url = new URL(document.URL);
  let certInfo = url.searchParams.getAll("cert");
  if (certInfo.length === 0) {
    await render(false, true);
    return;
  }
  certInfo = certInfo.map(cert => decodeURIComponent(cert));
  await buildChain(certInfo);
});

export const updateSelectedItem = (() => {
  let state;
  return selectedItem => {
    let certificateSection = document.querySelector("certificate-section");
    if (selectedItem) {
      if (state !== selectedItem) {
        state = selectedItem;
        certificateSection.updateCertificateSource(selectedItem);
        certificateSection.updateSelectedTab(selectedItem);
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
  let tabName = cert.subject ? cert.subject.cn || "" : "";

  if (!cert) {
    return {
      certItems,
      tabName,
    };
  }

  // Subject Name section
  if (cert.subject && cert.subject.entries) {
    certItems.push({
      sectionTitle: "Subject Name",
      sectionItems: cert.subject.entries.map(entry =>
        createEntryItem(entry[0], entry[1])
      ),
      Critical: false,
    });
  }

  // Issuer Name section
  if (cert.issuer && cert.issuer.entries) {
    certItems.push({
      sectionTitle: "Issuer Name",
      sectionItems: cert.issuer.entries.map(entry =>
        createEntryItem(entry[0], entry[1])
      ),
      Critical: false,
    });
  }

  // Validity section
  if (cert.notBefore && cert.notAfter) {
    certItems.push({
      sectionTitle: "Validity",
      sectionItems: [
        createEntryItem("Not Before", cert.notBefore),
        createEntryItem("Not Before UTC", cert.notBeforeUTC),
        createEntryItem("Not After", cert.notAfter),
        createEntryItem("Not After UTC", cert.notAfterUTC),
      ],
      Critical: false,
    });
  }

  // Subject Alt Names section
  if (cert.ext && cert.ext.san && cert.ext.san.altNames) {
    certItems.push({
      sectionTitle: "Subject Alt Names",
      sectionItems: cert.ext.san.altNames.map(entry =>
        createEntryItem(entry[0], entry[1])
      ),
      Critical: cert.ext.san.critical || false,
    });
  }

  // Public Key Info section
  if (cert.subjectPublicKeyInfo) {
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
  }

  // Miscellaneous section
  certItems.push({
    sectionTitle: "Miscellaneous",
    sectionItems: [
      createEntryItem("Serial Number", cert.serialNumber),
      createEntryItem(
        "Signature Algorithm",
        cert.signature ? cert.signature.name : null
      ),
      createEntryItem("Version", cert.version),
      createEntryItem("Download", cert.files.pem),
    ],
    Critical: false,
  });

  // Fingerprints section
  if (cert.fingerprint) {
    certItems.push({
      sectionTitle: "Fingerprints",
      sectionItems: [
        createEntryItem("SHA-256", cert.fingerprint.sha256),
        createEntryItem("SHA-1", cert.fingerprint.sha1),
      ],
      Critical: false,
    });
  }

  if (!cert.ext) {
    return {
      certItems,
      tabName,
    };
  }

  // Basic Constraints section
  certItems.push({
    sectionTitle: "Basic Constraints",
    sectionItems: [
      createEntryItem("Certificate Authority", cert.ext.basicConstraintscA),
    ],
    Critical: cert.ext.basicConstraints
      ? cert.ext.basicConstraints.critical
      : false,
  });

  // Key Usages section
  if (cert.ext.keyUsages) {
    certItems.push({
      sectionTitle: "Key Usages",
      sectionItems: [createEntryItem("Purposes", cert.ext.keyUsages.purposes)],
      Critical: cert.ext.keyUsages.critical || false,
    });
  }

  // Extended Key Usages section
  if (cert.ext.eKeyUsages) {
    certItems.push({
      sectionTitle: "Extended Key Usages",
      sectionItems: [createEntryItem("Purposes", cert.ext.eKeyUsages.purposes)],
      Critical: cert.ext.eKeyUsages.critical || false,
    });
  }

  // OCSP Stapling section
  if (cert.ext.ocspStaple) {
    certItems.push({
      sectionTitle: "OCSP Stapling",
      sectionItems: [createEntryItem("Required", cert.ext.ocspStaple.critical)],
      Critical: cert.ext.ocspStaple.critical || false,
    });
  }

  // Subject Key ID section
  if (cert.ext.sKID) {
    certItems.push({
      sectionTitle: "Subject Key ID",
      sectionItems: [createEntryItem("Key ID", cert.ext.sKID.id)],
      Critical: cert.ext.sKID.critical || false,
    });
  }

  // Authority Key ID section
  if (cert.ext.aKID) {
    certItems.push({
      sectionTitle: "Authority Key ID",
      sectionItems: [createEntryItem("Key ID", cert.ext.aKID.id)],
      Critical: cert.ext.aKID.critical || false,
    });
  }

  // CRL Endpoints section
  if (cert.ext.crlPoints && cert.ext.crlPoints.points) {
    certItems.push({
      sectionTitle: "CRL Endpoints",
      sectionItems: cert.ext.crlPoints.points.map(entry => {
        let label = "Distribution Point";
        return createEntryItem(label, entry);
      }),
      Critical: cert.ext.crlPoints.critical || false,
    });
  }

  let items = [];

  // // Authority Info (AIA) section
  if (cert.ext.aia && cert.ext.aia.descriptions) {
    cert.ext.aia.descriptions.forEach(entry => {
      items.push(createEntryItem("Location", entry.location));
      items.push(createEntryItem("Method", entry.method));
    });
    certItems.push({
      sectionTitle: "Authority Info (AIA)",
      sectionItems: items,
      Critical: cert.ext.aia.critical || false,
    });
  }

  // Certificate Policies section
  if (cert.ext.cp && cert.ext.cp.policies) {
    items = [];
    cert.ext.cp.policies.forEach(entry => {
      items.push(
        createEntryItem("Policy", entry.name + " ( " + entry.id + " )")
      );
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
  }

  // Embedded SCTs section
  if (cert.ext.scts && cert.ext.scts.timestamps) {
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
  }

  return {
    certItems,
    tabName,
  };
};

const render = async (certs, error) => {
  await customElements.whenDefined("certificate-section");
  const CertificateSection = customElements.get("certificate-section");
  document.querySelector("body").append(new CertificateSection(certs, error));
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
      let adjustedCerts = certs.map(cert => adjustCertInformation(cert));
      return render(adjustedCerts, false);
    })
    .catch(err => {
      render(null, true);
    });
};
