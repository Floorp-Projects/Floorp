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
  if (label == null || info == null) {
    return null;
  }
  return {
    label,
    info,
  };
};

const addToResultUsing = (callback, certItems, sectionTitle, Critical) => {
  let items = callback();
  if (items.length > 0) {
    certItems.push({
      sectionTitle,
      sectionItems: items,
      Critical,
    });
  }
};

const getElementByPathOrFalse = (obj, pathString) => {
  let pathArray = pathString.split(".");
  let result = obj;
  for (let entry of pathArray) {
    result = result[entry];
    if (result == null) {
      return false;
    }
  }
  return result ? result : false;
};

export const adjustCertInformation = cert => {
  let certItems = [];
  let tabName = cert.subject ? cert.subject.cn || "" : "";

  if (!cert) {
    return {
      certItems,
      tabName,
    };
  }

  addToResultUsing(
    () => {
      let items = [];
      if (cert.subject && cert.subject.entries) {
        items = cert.subject.entries
          .map(entry => createEntryItem(entry[0], entry[1]))
          .filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Subject Name",
    false
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.issuer && cert.issuer.entries) {
        items = cert.issuer.entries
          .map(entry => createEntryItem(entry[0], entry[1]))
          .filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Issuer Name",
    false
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.notBefore && cert.notAfter) {
        items = [
          createEntryItem("Not Before", cert.notBefore),
          createEntryItem("Not Before UTC", cert.notBeforeUTC),
          createEntryItem("Not After", cert.notAfter),
          createEntryItem("Not After UTC", cert.notAfterUTC),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Validity",
    false
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext && cert.ext.san && cert.ext.san.altNames) {
        items = cert.ext.san.altNames
          .map(entry => createEntryItem(entry[0], entry[1]))
          .filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Subject Alt Names",
    getElementByPathOrFalse(cert, "ext.san.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.subjectPublicKeyInfo) {
        items = [
          createEntryItem("Algorithm", cert.subjectPublicKeyInfo.kty),
          createEntryItem("Key size", cert.subjectPublicKeyInfo.keysize),
          createEntryItem("Curve", cert.subjectPublicKeyInfo.crv),
          createEntryItem("Public Value", cert.subjectPublicKeyInfo.xy),
          createEntryItem("Exponent", cert.subjectPublicKeyInfo.e),
          createEntryItem("Modulus", cert.subjectPublicKeyInfo.n),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Public Key Info",
    false
  );

  addToResultUsing(
    () => {
      let items = [
        createEntryItem("Serial Number", cert.serialNumber),
        createEntryItem(
          "Signature Algorithm",
          cert.signature ? cert.signature.name : null
        ),
        createEntryItem("Version", cert.version),
        createEntryItem("Download", cert.files ? cert.files.pem : null),
      ].filter(elem => elem != null);
      return items;
    },
    certItems,
    "Miscellaneous",
    false
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.fingerprint) {
        items = [
          createEntryItem("SHA-256", cert.fingerprint.sha256),
          createEntryItem("SHA-1", cert.fingerprint.sha1),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Fingerprints",
    false
  );

  if (!cert.ext) {
    return {
      certItems,
      tabName,
    };
  }

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.basicConstraints) {
        items = [
          createEntryItem(
            "Certificate Authority",
            cert.ext.basicConstraints.cA
          ),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Basic Constraints",
    getElementByPathOrFalse(cert, "ext.basicConstraints.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.keyUsages) {
        items = [
          createEntryItem("Purposes", cert.ext.keyUsages.purposes),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Key Usages",
    getElementByPathOrFalse(cert, "ext.keyUsages.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.eKeyUsages) {
        items = [
          createEntryItem("Purposes", cert.ext.eKeyUsages.purposes),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "Extended Key Usages",
    getElementByPathOrFalse(cert, "ext.eKeyUsages.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.ocspStaple) {
        items = [
          createEntryItem("Required", cert.ext.ocspStaple.critical),
        ].filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "OCSP Stapling",
    getElementByPathOrFalse(cert, "ext.ocspStaple.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.sKID) {
        items = [createEntryItem("Key ID", cert.ext.sKID.id)].filter(
          elem => elem != null
        );
      }
      return items;
    },
    certItems,
    "Subject Key ID",
    getElementByPathOrFalse(cert, "ext.sKID.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.aKID) {
        items = [createEntryItem("Key ID", cert.ext.aKID.id)].filter(
          elem => elem != null
        );
      }
      return items;
    },
    certItems,
    "Authority Key ID",
    getElementByPathOrFalse(cert, "ext.aKID.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.crlPoints && cert.ext.crlPoints.points) {
        items = cert.ext.crlPoints.points
          .map(entry => {
            let label = "Distribution Point";
            return createEntryItem(label, entry);
          })
          .filter(elem => elem != null);
      }
      return items;
    },
    certItems,
    "CRL Endpoints",
    getElementByPathOrFalse(cert, "ext.crlPoints.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.aia && cert.ext.aia.descriptions) {
        cert.ext.aia.descriptions.forEach(entry => {
          items.push(createEntryItem("Location", entry.location));
          items.push(createEntryItem("Method", entry.method));
        });
      }
      return items.filter(elem => elem != null);
    },
    certItems,
    "Authority Info (AIA)",
    getElementByPathOrFalse(cert, "ext.aia.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.cp && cert.ext.cp.policies) {
        cert.ext.cp.policies.forEach(entry => {
          if (entry.name && entry.id) {
            items.push(
              createEntryItem("Policy", entry.name + " ( " + entry.id + " )")
            );
          }
          items.push(createEntryItem("Value", entry.value));
          if (entry.qualifiers) {
            entry.qualifiers.forEach(qualifier => {
              if (qualifier.name && qualifier.id) {
                items.push(
                  createEntryItem(
                    "Qualifier",
                    qualifier.name + " ( " + qualifier.id + " )"
                  )
                );
              }
              items.push(createEntryItem("Value", qualifier.value));
            });
          }
        });
      }
      return items.filter(elem => elem != null);
    },
    certItems,
    "Certificate Policies",
    getElementByPathOrFalse(cert, "ext.cp.critical")
  );

  addToResultUsing(
    () => {
      let items = [];
      if (cert.ext.scts && cert.ext.scts.timestamps) {
        cert.ext.scts.timestamps.forEach(entry => {
          for (let key of Object.keys(entry)) {
            items.push(createEntryItem(key, entry[key]));
          }
        });
      }
      return items.filter(elem => elem != null);
    },
    certItems,
    "Embedded SCTs",
    getElementByPathOrFalse(cert, "ext.scts.critical")
  );

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
