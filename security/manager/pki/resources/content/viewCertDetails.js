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
 */

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsPK11TokenDB = "thayes@netscape.com/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;

function AddCertChain(node, chain)
{
  var idfier = "chain_";
  var child = [document.getElementById(node)];
  var item = document.createElement("treeitem");
  item.setAttribute("id", idfier + "0");
  item.setAttribute("container", "true");
  item.setAttribute("open", "true");
  var items = [item];
  var rows = [document.createElement("treerow")];
  var cell = document.createElement("treecell");
  cell.setAttribute("class", "treecell-indent");
  cell.setAttribute("value", chain[0]);
  var cells = [cell];
  for (var i=1; i<chain.length; i++) {
    child[i] = items[i-1];
    item = document.createElement("treeitem");
    item.setAttribute("id", idfier + i);
    item.setAttribute("container", "true");
    items[i] = item;
    rows[i] = document.createElement("treerow");
    cell = document.createElement("treecell");
    cell.setAttribute("class", "treecell-indent");
    cell.setAttribute("value", chain[i]);
    cells[i] = cell;
  }
  for (i=chain.length-1; i>=0; i--) {
    rows[i].appendChild(cells[i]);
    items[i].appendChild(rows[i]);
    child[i].appendChild(items[i]);
  }
}

function setWindowName()
{
  myName = self.name;
  var windowReference=document.getElementById('certDetails');
  windowReference.setAttribute("title","Certificate Detail: \""+myName+"\"");

  //  Get the token
  //  XXX ignore this for now.  NSS will find the cert on a token
  //      by "tokenname:certname", which is what we have.
  //var tokenName = "";
  //var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  //var token = pk11db.findTokenByName(tokenName);

  //  Get the cert from the cert database
  var certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  //var cert = certdb.getCertByNickname(token, myName);
  var cert = certdb.getCertByNickname(null, myName);

  //
  //  Set the cert attributes for viewing
  //

  //  The chain of trust
  var chainEnum = cert.getChain();
  chainEnum.first();
  var c = 0;
  var chain = [];
  try {
  while (true) {
    var node = chainEnum.currentItem();
    node = node.QueryInterface(nsIX509Cert);
    chain[c++] = node.commonName;
    chainEnum.next();
  }
  } catch (e) {}
  AddCertChain("chain", chain.reverse());

  //  Common Name
  var cn=document.getElementById('commonname');
  cn.setAttribute("value", cert.commonName);

  //  Organization
  var org=document.getElementById('organization');
  org.setAttribute("value", cert.organization);

  //  Organizational Unit
  var ou=document.getElementById('orgunit');
  ou.setAttribute("value", cert.organizationalUnit);
}
