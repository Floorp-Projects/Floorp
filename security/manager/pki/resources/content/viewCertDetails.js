/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 */

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;
const nsIASN1Object = Components.interfaces.nsIASN1Object;
const nsIASN1Sequence = Components.interfaces.nsIASN1Sequence;
const nsIASN1PrintableItem = Components.interfaces.nsIASN1PrintableItem;
const nsIASN1Outliner = Components.interfaces.nsIASN1Outliner;
const nsASN1Outliner = "@mozilla.org/security/nsASN1Outliner;1"

var bundle;

function AddCertChain(node, chain, idPrefix)
{
  var idfier = idPrefix+"chain_";
  var child = document.getElementById(node);
  var numCerts = chain.Count();
  var currCert;
  var displayVal;
  var addTwistie;
  for (var i=numCerts-1; i>=0; i--) {
    currCert = chain.GetElementAt(i);
    currCert = currCert.QueryInterface(nsIX509Cert);
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
  var windowReference=document.getElementById('certDetails');
  var myName = self.name;
  bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var cert;

  var certDetails = bundle.GetStringFromName('certDetails');
  if (myName != "_blank") {
    windowReference.setAttribute("title",certDetails+'"'+myName+'"');
    //  Get the token
    //  XXX ignore this for now.  NSS will find the cert on a token
    //      by "tokenname:certname", which is what we have.
    //var tokenName = "";
    //var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
    //var token = pk11db.findTokenByName(tokenName);

    //var cert = certdb.getCertByNickname(token, myName);
    cert = certdb.getCertByNickname(null, myName);
  } else {
    var pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);
    var isupport = pkiParams.getISupportAtIndex(1);
    cert = isupport.QueryInterface(nsIX509Cert);
    windowReference.setAttribute("title", 
                                 certDetails+'"'+cert.windowTitle+'"');
  }

  //
  //  Set the cert attributes for viewing
  //

  //  The chain of trust
  var chain = cert.getChain();
  AddCertChain("chainDump", chain,"dump_");
  DisplayGeneralDataFromCert(cert);
  BuildPrettyPrint(cert);
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
  treeCell.setAttribute("class", "treecell-indent");
  treeCell.setAttribute("label",label);
  if (value)
    treeCell.setAttribute("display",value);
  treeRow.appendChild(treeCell);
  treeElem1.appendChild(treeRow);
  treeChild.appendChild(treeElem1);
  return treeElem1;
}

function displaySelected() {
  var asn1Outliner = document.getElementById('prettyDumpOutliner').
                     outlinerBoxObject.view.QueryInterface(nsIASN1Outliner);
  var items = asn1Outliner.selection;
  var certDumpVal = document.getElementById('certDumpVal');
  if (items.currentIndex != -1) {
    var value = asn1Outliner.getDisplayData(items.currentIndex);
    certDumpVal.value = value;
  } else {
    certDumpVal.value ="";
  }
}

function BuildPrettyPrint(cert)
{
  var certDumpOutliner = Components.classes[nsASN1Outliner].
                          createInstance(nsIASN1Outliner);
  certDumpOutliner.loadASN1Structure(cert.ASN1Structure);
  document.getElementById('prettyDumpOutliner').
           outlinerBoxObject.view =  certDumpOutliner;
}

function addAttributeFromCert(nodeName, value)
{
  var node = document.getElementById(nodeName);
  if (!value) {
    value = bundle.GetStringFromName('notPresent');  
  }
  node.setAttribute('value',value)
}

function DisplayGeneralDataFromCert(cert)
{
  //  Verification and usage
  var verifystr = "";
  var o1 = {};
  var o2 = {};
  var o3 = {};
  cert.getUsages(o1, o2, o3);
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
  } else { /* if (verifystate == cert.NOT_VERIFIED_UNKNOWN) */
    verifystr = bundle.GetStringFromName('certNotVerified_Unknown');
  }
  var verified=document.getElementById('verified');
  verified.setAttribute("value", verifystr);
  if (count > 0) {
    var verifyInfoBox = document.getElementById('verify_info_box');
    for (var i=0; i<count; i++) {
      AddUsage(usageList[i],verifyInfoBox);
    }
  }

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
  addAttributeFromCert('validitystart', cert.issuedDate);
  // Validity end
  addAttributeFromCert('validityend', cert.expiresDate);
  
  //Now to populate the fields that correspond to the issuer.
  var issuerCommonname, issuerOrg, issuerOrgUnit;
  issuerCommonname = cert.issuerCommonName;
  issuerOrg = cert.issuerOrganization;
  issuerOrgUnit = cert.issuerOrganizationalUnit;
  addAttributeFromCert('issuercommonname', issuerCommonname);
  addAttributeFromCert('issuerorganization', issuerOrg);
  addAttributeFromCert('issuerorgunit', issuerOrgUnit);
}

function updateCertDump()
{
  var asn1Outliner = document.getElementById('prettyDumpOutliner').
                     outlinerBoxObject.view.QueryInterface(nsIASN1Outliner);

  var tree = document.getElementById('treesetDump');
  var items=tree.selectedItems;

  if (items.length==0) {
    alert("No items are selected."); //This should never happen.
  } else {
    var dbKey = items[0].firstChild.firstChild.getAttribute('display');
    //  Get the cert from the cert database
    var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
    var cert = certdb.getCertByDBKey(dbKey,null);
    asn1Outliner.loadASN1Structure(cert.ASN1Structure);
  }
  displaySelected();
}
