/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIASN1Object = Components.interfaces.nsIASN1Object;
const nsIASN1Sequence = Components.interfaces.nsIASN1Sequence;
const nsIASN1PrintableItem = Components.interfaces.nsIASN1PrintableItem;
const nsIASN1Tree = Components.interfaces.nsIASN1Tree;
const nsASN1Tree = "@mozilla.org/security/nsASN1Tree;1";
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var bundle;

function doPrompt(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

/**
 * Fills out the "Certificate Hierarchy" tree of the cert viewer "Details" tab.
 *
 * @param {tree} node
 *        Parent tree node to append to.
 * @param {nsIArray<nsIX509Cert>} chain
 *        Chain where cert element n is issued by cert element n + 1.
 */
function AddCertChain(node, chain)
{
  var child = document.getElementById(node);
  var currCert;
  var displayVal;
  for (let i = chain.length - 1; i >= 0; i--) {
    currCert = chain.queryElementAt(i, nsIX509Cert);
    if (currCert.commonName) {
      displayVal = currCert.commonName;
    } else {
      displayVal = currCert.windowTitle;
    }
    let addTwistie = i != 0;
    child = addChildrenToTree(child, displayVal, currCert.dbKey, addTwistie);
  }
}

/**
 * Adds a "verified usage" of a cert to the "General" tab of the cert viewer.
 *
 * @param {String} usage
 *        Verified usage to add.
 * @param {Node} verifyInfoBox
 *        Parent node to append to.
 */
function AddUsage(usage, verifyInfoBox)
{
  let text = document.createElement("textbox");
  text.setAttribute("value", usage);
  text.setAttribute("style", "margin: 2px 5px");
  text.setAttribute("readonly", "true");
  text.setAttribute("class", "scrollfield");
  verifyInfoBox.appendChild(text);
}

function setWindowName()
{
  //  Get the cert from the cert database
  var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  var myName = self.name;
  bundle = document.getElementById("pippki_bundle");
  var cert;

  var certDetails = bundle.getString('certDetails');
  if (myName != "") {
    document.title = certDetails + '"' + myName + '"'; // XXX l10n?
    cert = certdb.findCertByNickname(myName);
  } else {
    var params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    cert = params.objects.queryElementAt(0, nsIX509Cert);
    document.title = certDetails + '"' + cert.windowTitle + '"'; // XXX l10n?
  }

  //
  //  Set the cert attributes for viewing
  //

  //  The chain of trust
  AddCertChain("treesetDump", cert.getChain());
  DisplayGeneralDataFromCert(cert);
  BuildPrettyPrint(cert);
  cert.requestUsagesArrayAsync(new listener());
}

function addChildrenToTree(parentTree, label, value, addTwistie)
{
  let treeChild1 = document.createElement("treechildren");
  let treeElement = addTreeItemToTreeChild(treeChild1, label, value,
                                           addTwistie);
  parentTree.appendChild(treeChild1);
  return treeElement;
}

function addTreeItemToTreeChild(treeChild, label, value, addTwistie)
{
  let treeElem1 = document.createElement("treeitem");
  if (addTwistie) {
    treeElem1.setAttribute("container", "true");
    treeElem1.setAttribute("open", "true");
  }
  let treeRow = document.createElement("treerow");
  let treeCell = document.createElement("treecell");
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
  var asn1Tree = document.getElementById('prettyDumpTree')
          .view.QueryInterface(nsIASN1Tree);
  var items = asn1Tree.selection;
  var certDumpVal = document.getElementById('certDumpVal');
  if (items.currentIndex != -1) {
    var value = asn1Tree.getDisplayData(items.currentIndex);
    certDumpVal.value = value;
  } else {
    certDumpVal.value = "";
  }
}

function BuildPrettyPrint(cert)
{
  var certDumpTree = Components.classes[nsASN1Tree].
                          createInstance(nsIASN1Tree);
  certDumpTree.loadASN1Structure(cert.ASN1Structure);
  document.getElementById('prettyDumpTree').view = certDumpTree;
}

function addAttributeFromCert(nodeName, value)
{
  var node = document.getElementById(nodeName);
  if (!value) {
    value = bundle.getString('notPresent');
  }
  node.setAttribute('value', value);
}



function listener() {
}

listener.prototype.QueryInterface =
  function(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsICertVerificationListener)) {
      return this;
    }

    throw new Error(Components.results.NS_ERROR_NO_INTERFACE);
  };

listener.prototype.notify =
  function(cert, result) {
    DisplayVerificationData(cert, result);
  };

function DisplayVerificationData(cert, result)
{
  document.getElementById("verify_pending").setAttribute("hidden", "true");

  if (!result || !cert) {
    return; // no results could be produced
  }

  if (!(cert instanceof Components.interfaces.nsIX509Cert)) {
    return;
  }

  //  Verification and usage
  var verifystr = "";
  var o1 = {};
  var o2 = {};
  var o3 = {};

  if (!(result instanceof Components.interfaces.nsICertVerificationResult)) {
    return;
  }

  result.getUsagesArrayResult(o1, o2, o3);

  var verifystate = o1.value;
  var count = o2.value;
  var usageList = o3.value;
  if (verifystate == cert.VERIFIED_OK) {
    verifystr = bundle.getString('certVerified');
  } else if (verifystate == cert.CERT_REVOKED) {
    verifystr = bundle.getString('certNotVerified_CertRevoked');
  } else if (verifystate == cert.CERT_EXPIRED) {
    verifystr = bundle.getString('certNotVerified_CertExpired');
  } else if (verifystate == cert.CERT_NOT_TRUSTED) {
    verifystr = bundle.getString('certNotVerified_CertNotTrusted');
  } else if (verifystate == cert.ISSUER_NOT_TRUSTED) {
    verifystr = bundle.getString('certNotVerified_IssuerNotTrusted');
  } else if (verifystate == cert.ISSUER_UNKNOWN) {
    verifystr = bundle.getString('certNotVerified_IssuerUnknown');
  } else if (verifystate == cert.INVALID_CA) {
    verifystr = bundle.getString('certNotVerified_CAInvalid');
  } else if (verifystate == cert.SIGNATURE_ALGORITHM_DISABLED) {
    verifystr = bundle.getString('certNotVerified_AlgorithmDisabled');
  } else { /* if (verifystate == cert.NOT_VERIFIED_UNKNOWN || == USAGE_NOT_ALLOWED) */
    verifystr = bundle.getString('certNotVerified_Unknown');
  }
  let verified = document.getElementById("verified");
  verified.textContent = verifystr;
  if (count > 0) {
    var verifyInfoBox = document.getElementById('verify_info_box');
    for (let i = 0; i < count; i++) {
      AddUsage(usageList[i], verifyInfoBox);
    }
  }
}

/**
 * Displays information about a cert in the "General" tab of the cert viewer.
 *
 * @param {nsIX509Cert} cert
 *        Cert to display information about.
 */
function DisplayGeneralDataFromCert(cert)
{
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

function updateCertDump()
{
  var asn1Tree = document.getElementById('prettyDumpTree')
          .view.QueryInterface(nsIASN1Tree);

  var tree = document.getElementById('treesetDump');
  if (tree.currentIndex < 0) {
    doPrompt("No items are selected."); //This should never happen.
  } else {
    var item = tree.contentView.getItemAtIndex(tree.currentIndex);
    var dbKey = item.firstChild.firstChild.getAttribute('display');
    //  Get the cert from the cert database
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    asn1Tree.loadASN1Structure(cert.ASN1Structure);
  }
  displaySelected();
}

function getCurrentCert()
{
  var realIndex;
  var tree = document.getElementById('treesetDump');
  if (tree.view.selection.isSelected(tree.currentIndex)
      && document.getElementById('prettyprint_tab').selected) {
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
    var item = tree.contentView.getItemAtIndex(realIndex);
    var dbKey = item.firstChild.firstChild.getAttribute('display');
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey);
    return cert;
  }
  /* shouldn't really happen */
  return null;
}
