/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bob Lord <lord@netscape.com>
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *   Kaspar Brand <mozcontrib@velox.ch>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsIX509Cert3 = Components.interfaces.nsIX509Cert3;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;
const nsIASN1Object = Components.interfaces.nsIASN1Object;
const nsIASN1Sequence = Components.interfaces.nsIASN1Sequence;
const nsIASN1PrintableItem = Components.interfaces.nsIASN1PrintableItem;
const nsIASN1Tree = Components.interfaces.nsIASN1Tree;
const nsASN1Tree = "@mozilla.org/security/nsASN1Tree;1"

var bundle;

function doPrompt(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

function AddCertChain(node, chain, idPrefix)
{
  var idfier = idPrefix+"chain_";
  var child = document.getElementById(node);
  var numCerts = chain.length;
  var currCert;
  var displayVal;
  var addTwistie;
  for (var i=numCerts-1; i>=0; i--) {
    currCert = chain.queryElementAt(i, nsIX509Cert);
    if (currCert.commonName) {
      displayVal = currCert.commonName;
    } else {
      displayVal = currCert.windowTitle;
    }
    if (0 == i) {
      addTwistie = false;
    } else {
      addTwistie = true;
    }
    child = addChildrenToTree(child, displayVal, currCert.dbKey,addTwistie);
  }
}

function AddUsage(usage,verifyInfoBox)
{
  var text  = document.createElement("textbox");
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
  bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var cert;

  var certDetails = bundle.GetStringFromName('certDetails');
  if (myName != "") {
    document.title = certDetails + '"' + myName + '"'; // XXX l10n?
    //  Get the token
    //  XXX ignore this for now.  NSS will find the cert on a token
    //      by "tokenname:certname", which is what we have.
    //var tokenName = "";
    //var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
    //var token = pk11db.findTokenByName(tokenName);

    //var cert = certdb.findCertByNickname(token, myName);
    cert = certdb.findCertByNickname(null, myName);
  } else {
    var pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);
    var isupport = pkiParams.getISupportAtIndex(1);
    cert = isupport.QueryInterface(nsIX509Cert);
    document.title = certDetails + '"' + cert.windowTitle + '"'; // XXX l10n?
  }

  //
  //  Set the cert attributes for viewing
  //

  //  The chain of trust
  var chain = cert.getChain();
  AddCertChain("treesetDump", chain, "dump_");
  DisplayGeneralDataFromCert(cert);
  BuildPrettyPrint(cert);
  
  if (cert instanceof nsIX509Cert3)
  {
    cert.requestUsagesArrayAsync(new listener());
  }
}

 
function addChildrenToTree(parentTree,label,value,addTwistie)
{
  var treeChild1 = document.createElement("treechildren");
  var treeElement = addTreeItemToTreeChild(treeChild1,label,value,addTwistie);
  parentTree.appendChild(treeChild1);
  return treeElement;
}

function addTreeItemToTreeChild(treeChild,label,value,addTwistie)
{
  var treeElem1 = document.createElement("treeitem");
  if (addTwistie) {
    treeElem1.setAttribute("container","true");
    treeElem1.setAttribute("open","true");
  }
  var treeRow = document.createElement("treerow");
  var treeCell = document.createElement("treecell");
  treeCell.setAttribute("label",label);
  if (value)
    treeCell.setAttribute("display",value);
  treeRow.appendChild(treeCell);
  treeElem1.appendChild(treeRow);
  treeChild.appendChild(treeElem1);
  return treeElem1;
}

function displaySelected() {
  var asn1Tree = document.getElementById('prettyDumpTree').
                     treeBoxObject.view.QueryInterface(nsIASN1Tree);
  var items = asn1Tree.selection;
  var certDumpVal = document.getElementById('certDumpVal');
  if (items.currentIndex != -1) {
    var value = asn1Tree.getDisplayData(items.currentIndex);
    certDumpVal.value = value;
  } else {
    certDumpVal.value ="";
  }
}

function BuildPrettyPrint(cert)
{
  var certDumpTree = Components.classes[nsASN1Tree].
                          createInstance(nsIASN1Tree);
  certDumpTree.loadASN1Structure(cert.ASN1Structure);
  document.getElementById('prettyDumpTree').
           treeBoxObject.view =  certDumpTree;
}

function addAttributeFromCert(nodeName, value)
{
  var node = document.getElementById(nodeName);
  if (!value) {
    value = bundle.GetStringFromName('notPresent');  
  }
  node.setAttribute('value',value)
}



function listener() {
}

listener.prototype.QueryInterface =
  function(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsICertVerificationListener))
        return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }

listener.prototype.notify =
  function(cert, result) {
    DisplayVerificationData(cert, result);
  }

function DisplayVerificationData(cert, result)
{
  document.getElementById("verify_pending").setAttribute("hidden", "true");

  if (!result || !cert)
    return; // no results could be produced

  if (!(cert instanceof Components.interfaces.nsIX509Cert))
    return;

  //  Verification and usage
  var verifystr = "";
  var o1 = {};
  var o2 = {};
  var o3 = {};

  if (!(result instanceof Components.interfaces.nsICertVerificationResult))
    return;

  result.getUsagesArrayResult(o1, o2, o3);

  var verifystate = o1.value;
  var count = o2.value;
  var usageList = o3.value;
  if (verifystate == cert.VERIFIED_OK) {
    verifystr = bundle.GetStringFromName('certVerified');
  } else if (verifystate == cert.CERT_REVOKED) {
    verifystr = bundle.GetStringFromName('certNotVerified_CertRevoked');
  } else if (verifystate == cert.CERT_EXPIRED) {
    verifystr = bundle.GetStringFromName('certNotVerified_CertExpired');
  } else if (verifystate == cert.CERT_NOT_TRUSTED) {
    verifystr = bundle.GetStringFromName('certNotVerified_CertNotTrusted');
  } else if (verifystate == cert.ISSUER_NOT_TRUSTED) {
    verifystr = bundle.GetStringFromName('certNotVerified_IssuerNotTrusted');
  } else if (verifystate == cert.ISSUER_UNKNOWN) {
    verifystr = bundle.GetStringFromName('certNotVerified_IssuerUnknown');
  } else if (verifystate == cert.INVALID_CA) {
    verifystr = bundle.GetStringFromName('certNotVerified_CAInvalid');
  } else { /* if (verifystate == cert.NOT_VERIFIED_UNKNOWN || == USAGE_NOT_ALLOWED) */
    verifystr = bundle.GetStringFromName('certNotVerified_Unknown');
  }
  var verified=document.getElementById('verified');
  verified.textContent = verifystr;
  if (count > 0) {
    var verifyInfoBox = document.getElementById('verify_info_box');
    for (var i=0; i<count; i++) {
      AddUsage(usageList[i],verifyInfoBox);
    }
  }
}

function DisplayGeneralDataFromCert(cert)
{
  //  Common Name
  addAttributeFromCert('commonname', cert.commonName);
  //  Organization
  addAttributeFromCert('organization', cert.organization);
  //  Organizational Unit
  addAttributeFromCert('orgunit', cert.organizationalUnit);
  //  Serial Number
  addAttributeFromCert('serialnumber',cert.serialNumber);
  //  SHA1 Fingerprint
  addAttributeFromCert('sha1fingerprint',cert.sha1Fingerprint);
  //  MD5 Fingerprint
  addAttributeFromCert('md5fingerprint',cert.md5Fingerprint);
  // Validity start
  addAttributeFromCert('validitystart', cert.validity.notBeforeLocalDay);
  // Validity end
  addAttributeFromCert('validityend', cert.validity.notAfterLocalDay);
  
  //Now to populate the fields that correspond to the issuer.
  var issuerCommonname, issuerOrg, issuerOrgUnit;
  issuerCommonname = cert.issuerCommonName;
  issuerOrg = cert.issuerOrganization;
  issuerOrgUnit = cert.issuerOrganizationUnit;
  addAttributeFromCert('issuercommonname', issuerCommonname);
  addAttributeFromCert('issuerorganization', issuerOrg);
  addAttributeFromCert('issuerorgunit', issuerOrgUnit);
}

function updateCertDump()
{
  var asn1Tree = document.getElementById('prettyDumpTree').
                     treeBoxObject.view.QueryInterface(nsIASN1Tree);

  var tree = document.getElementById('treesetDump');
  if (tree.currentIndex < 0) {
    doPrompt("No items are selected."); //This should never happen.
  } else {
    var item = tree.contentView.getItemAtIndex(tree.currentIndex);
    var dbKey = item.firstChild.firstChild.getAttribute('display');
    //  Get the cert from the cert database
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.findCertByDBKey(dbKey,null);
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
    var cert = certdb.findCertByDBKey(dbKey,null);
    return cert;
  }
  /* shouldn't really happen */
  return null;
}
