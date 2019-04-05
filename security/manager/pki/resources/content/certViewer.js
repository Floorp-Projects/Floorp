/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements functionality for certViewer.xul and its general and details
 *       tabs.
 * @argument {nsISupports} window.arguments[0]
 *           The cert to view, queryable to nsIX509Cert.
 */

/**
 * Fills out the "Certificate Hierarchy" tree of the cert viewer "Details" tab.
 *
 * @param {tree} node
 *        Parent tree node to append to.
 * @param {Array} chain
 *        An array of nsIX509Cert where cert n is issued by cert n + 1.
 */
function AddCertChain(node, chain) {
  if (!chain || chain.length < 1) {
    return;
  }
  let child = document.getElementById(node);
  // Clear any previous state.
  let preexistingChildren = child.querySelectorAll("treechildren");
  for (let preexistingChild of preexistingChildren) {
    child.removeChild(preexistingChild);
  }
  for (let i = chain.length - 1; i >= 0; i--) {
    let currCert = chain[i];
    let displayValue = currCert.displayName;
    let addTwistie = i != 0;
    child = addChildrenToTree(child, displayValue, currCert.dbKey, addTwistie);
  }
}

/**
 * Adds a "verified usage" of a cert to the "General" tab of the cert viewer.
 *
 * @param {String} l10nId
 *        l10nId of verified usage to add.
 */
function AddUsage(l10nId) {
  let verifyInfoBox = document.getElementById("verify_info_box");
  let text = document.createXULElement("textbox");
  document.l10n.setAttributes(text, l10nId);
  text.setAttribute("data-l10n-attrs", "value");
  text.setAttribute("style", "margin: 2px 5px");
  text.setAttribute("readonly", "true");
  text.setAttribute("class", "scrollfield");
  verifyInfoBox.appendChild(text);
}

function setWindowName() {
  let cert = window.arguments[0].QueryInterface(Ci.nsIX509Cert);
  window.document.l10n.setAttributes(window.document.documentElement, "cert-viewer-title", {certName: cert.displayName});

  //
  //  Set the cert attributes for viewing
  //

  // Set initial dummy chain of just the cert itself. A more complete chain (if
  // one can be found), will be set when the promise chain beginning at
  // asyncDetermineUsages finishes.
  AddCertChain("treesetDump", [cert]);
  DisplayGeneralDataFromCert(cert);
  BuildPrettyPrint(cert);

  asyncDetermineUsages(cert).then(displayUsages);
}

// Map of certificate usage name to localization identifier.
const certificateUsageToStringBundleName = {
  certificateUsageSSLClient: "verify-ssl-client",
  certificateUsageSSLServer: "verify-ssl-server",
  certificateUsageSSLCA: "verify-ssl-ca",
  certificateUsageEmailSigner: "verify-email-signer",
  certificateUsageEmailRecipient: "verify-email-recip",
};

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SEC_ERROR_EXPIRED_CERTIFICATE                     = SEC_ERROR_BASE + 11;
const SEC_ERROR_REVOKED_CERTIFICATE                     = SEC_ERROR_BASE + 12;
const SEC_ERROR_UNKNOWN_ISSUER                          = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                        = SEC_ERROR_BASE + 20;
const SEC_ERROR_UNTRUSTED_CERT                          = SEC_ERROR_BASE + 21;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE              = SEC_ERROR_BASE + 30;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED       = SEC_ERROR_BASE + 176;


/**
 * Updates the usage display area given the results from asyncDetermineUsages.
 *
 * @param {Array} results
 *        An array of objects with the properties "usageString", "errorCode",
 *        and "chain".
 *        usageString is a string that is a key in the certificateUsages map.
 *        errorCode is either an NSPR error code or PRErrorCodeSuccess (which is
 *        a pseudo-NSPR error code with the value 0 that indicates success).
 *        chain is the built trust path, if one was found
 */
function displayUsages(results) {
  document.getElementById("verify_pending").setAttribute("hidden", "true");
  let verified = document.getElementById("verified");
  let someSuccess = results.some(result =>
    result.errorCode == PRErrorCodeSuccess
  );
  if (someSuccess) {
    document.l10n.setAttributes(verified, "cert-verified");
    results.forEach(result => {
      if (result.errorCode != PRErrorCodeSuccess) {
        return;
      }
      let usageL10nId = certificateUsageToStringBundleName[result.usageString];
      AddUsage(usageL10nId);
    });
    AddCertChain("treesetDump", getBestChain(results));
  } else {
    const errorRankings = [
      { error: SEC_ERROR_REVOKED_CERTIFICATE,
        bundleString: "cert-not-verified-cert-revoked" },
      { error: SEC_ERROR_UNTRUSTED_CERT,
        bundleString: "cert-not-verified-cert-not-trusted" },
      { error: SEC_ERROR_UNTRUSTED_ISSUER,
        bundleString: "cert-not-verified-issuer-not-trusted" },
      { error: SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
        bundleString: "cert-not-verified_algorithm-disabled" },
      { error: SEC_ERROR_EXPIRED_CERTIFICATE,
        bundleString: "cert-not-verified-cert-expired" },
      { error: SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE,
        bundleString: "cert-not-verified-ca-invalid" },
      { error: SEC_ERROR_UNKNOWN_ISSUER,
        bundleString: "cert-not-verified-issuer-unknown" },
    ];
    let errorPresentFlag = false;
    for (let errorRanking of errorRankings) {
      let errorPresent = results.some(result =>
        result.errorCode == errorRanking.error
      );
      if (errorPresent) {
        document.l10n.setAttributes(verified, errorRanking.bundleString);
        errorPresentFlag = true;
        break;
      }
    }
    if (!errorPresentFlag) {
      document.l10n.setAttributes(verified, "cert-not-verified-unknown");
    }
  }
  // Notify that we are done determining the certificate's valid usages (this
  // should be treated as an implementation detail that enables tests to run
  // efficiently - other code in the browser probably shouldn't rely on this).
  Services.obs.notifyObservers(window, "ViewCertDetails:CertUsagesDone");
}

function addChildrenToTree(parentTree, label, value, addTwistie) {
  let treeChild1 = document.createXULElement("treechildren");
  let treeElement = addTreeItemToTreeChild(treeChild1, label, value,
                                           addTwistie);
  parentTree.appendChild(treeChild1);
  return treeElement;
}

function addTreeItemToTreeChild(treeChild, label, value, addTwistie) {
  let treeElem1 = document.createXULElement("treeitem");
  if (addTwistie) {
    treeElem1.setAttribute("container", "true");
    treeElem1.setAttribute("open", "true");
  }
  let treeRow = document.createXULElement("treerow");
  let treeCell = document.createXULElement("treecell");
  treeCell.setAttribute("label", label);
  if (value) {
    treeCell.setAttribute("display", value);
  }
  treeRow.appendChild(treeCell);
  treeElem1.appendChild(treeRow);
  treeChild.appendChild(treeElem1);
  return treeElem1;
}

function displaySelected() {
  var asn1Tree = document.getElementById("prettyDumpTree")
          .view.QueryInterface(Ci.nsIASN1Tree);
  var items = asn1Tree.selection;
  var certDumpVal = document.getElementById("certDumpVal");
  if (items.currentIndex != -1) {
    var value = asn1Tree.getDisplayData(items.currentIndex);
    certDumpVal.value = value;
  } else {
    certDumpVal.value = "";
  }
}

function BuildPrettyPrint(cert) {
  var certDumpTree = Cc["@mozilla.org/security/nsASN1Tree;1"].
                          createInstance(Ci.nsIASN1Tree);
  certDumpTree.loadASN1Structure(cert.ASN1Structure);
  document.getElementById("prettyDumpTree").view = certDumpTree;
}

function addAttributeFromCert(nodeName, value) {
  var node = document.getElementById(nodeName);
  if (!value) {
    document.l10n.setAttributes(node, "not-present");
    return;
  }
  node.value = value;
}

/**
 * Displays information about a cert in the "General" tab of the cert viewer.
 *
 * @param {nsIX509Cert} cert
 *        Cert to display information about.
 */
function DisplayGeneralDataFromCert(cert) {
  addAttributeFromCert("commonname", cert.commonName);
  addAttributeFromCert("organization", cert.organization);
  addAttributeFromCert("orgunit", cert.organizationalUnit);
  addAttributeFromCert("serialnumber", cert.serialNumber);
  addAttributeFromCert("sha256fingerprint", cert.sha256Fingerprint);
  addAttributeFromCert("sha1fingerprint", cert.sha1Fingerprint);
  addAttributeFromCert("validitystart", cert.validity.notBeforeLocalDay);
  addAttributeFromCert("validityend", cert.validity.notAfterLocalDay);

  addAttributeFromCert("issuercommonname", cert.issuerCommonName);
  addAttributeFromCert("issuerorganization", cert.issuerOrganization);
  addAttributeFromCert("issuerorgunit", cert.issuerOrganizationUnit);
}

function updateCertDump() {
  var asn1Tree = document.getElementById("prettyDumpTree")
          .view.QueryInterface(Ci.nsIASN1Tree);

  var tree = document.getElementById("treesetDump");
  if (tree.currentIndex >= 0) {
    var item = tree.view.getItemAtIndex(tree.currentIndex);
    var dbKey = item.firstChild.firstChild.getAttribute("display");
    //  Get the cert from the cert database
    var certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    asn1Tree.loadASN1Structure(cert.ASN1Structure);
  }
  displaySelected();
}

function getCurrentCert() {
  var realIndex;
  var tree = document.getElementById("treesetDump");
  if (tree.view.selection.isSelected(tree.currentIndex)
      && document.getElementById("prettyprint_tab").selected) {
    /* if the user manually selected a cert on the Details tab,
       then take that one  */
    realIndex = tree.currentIndex;
  } else {
    /* otherwise, take the one at the bottom of the chain
       (i.e. the one of the end-entity, unless we're displaying
       CA certs) */
    realIndex = tree.view.rowCount - 1;
  }
  if (realIndex >= 0) {
    var item = tree.view.getItemAtIndex(realIndex);
    var dbKey = item.firstChild.firstChild.getAttribute("display");
    var certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    return cert;
  }
  /* shouldn't really happen */
  return null;
}
