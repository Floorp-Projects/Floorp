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

function AddItem(children, cells, prefix, idfier)
{
  var kids = document.getElementById(children);
  var item = document.createElement("treeitem");
  var row  = document.createElement("treerow");
  for (var i=0; i<cells.length; i++) {
    var cell = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("value", cells[i]);
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id", prefix + idfier);
  kids.appendChild(item);
}

function LoadCertNames()
{
  certmgr = Components
            .classes["@mozilla.org/security/certmanager;1"]
            .createInstance();
  certmgr = certmgr.QueryInterface(Components
                                   .interfaces
                                   .nsICertificateManager);
  certNameList = certmgr.getCertNicknames(1);
  if (certNameList.length > 0) {
    var delim = certNameList[0];
    certNameList = certNameList.split(delim);
    certNameList.sort();
  }
  var nb = 0;
  var nm = 0;
  for (var i=1; i<certNameList.length; i++) {
    var certname = certNameList[i];
    var certname = certNameList[i];
    var ti = certname.indexOf(":");
    var token = "";
    if (ti > 0) {
      token = certname.substring(0, ti);
      certname = certname.substring(ti+1, certname.length);
    }
    if (token == "Builtin Object Token") {
      AddItem("builtins", [certname], "builtin_", nb);
      nb++;
    } else {
      AddItem("mycas", [certname], "myca_", nm);
      nm++;
    }
  }
}

function enableButtons()
{
  var tree = document.getElementById('treeset');
  var items = tree.selectedItems;
  var toggle="true";
  if (items.length == 0) {
    toggle="true";
  } else {
    toggle="false";
  }
/*
  var enablebackupbutton=document.getElementById('backupButton');
  enablebackupbutton.setAttribute("disabled",toggle);
*/
  var enableViewButton=document.getElementById('viewButton');
  enableViewButton.setAttribute("disabled",toggle);
  var enableDeleteButton=document.getElementById('deleteButton');
  enableDeleteButton.setAttribute("disabled",toggle);
}

function doBackup()
{
  var tree = document.getElementById('treeset');
  var items = tree.selectedItems;
  if (items.length==0){
//    alert("No items are selected.");
    return;
  } else {
    txt="(Insert real dialog box here)\nYou want to view these certificates:\n\n";
    for (t=0; t<items.length; t++) {
      txt += items[t].firstChild.firstChild.getAttribute('value')+'\n';
    }
    alert(txt);
  }
}

function doBackupAll()
{
  // Select all rows, then call doBackup()
  var tree = document.getElementById('treeset');
  tree.selectAll();
  doBackup();
}


function changePassword()
{
//  window.open("changepassword.xul","pwchange", "chrome,width=300,height=350,resizable=0,modal=1,dialog=1");
}

function deleteCerts()
{
  var tree = document.getElementById('treeset');
  var items = tree.selectedItems;
  if (items.length == 0) alert("No items are selected.");
  else {
    txt = "You want to delete these certificates:\n\n";
    for (t=0; t<items.length; t++){
      txt+=items[t].firstChild.firstChild.getAttribute('value')+'\n';
    }
    alert(txt);
  }
}


function viewCerts()
{
  var tree = document.getElementById('treeset');
  var items = tree.selectedItems;
  if (items.length == 0) {
    alert("No items are selected.");
    return;
  } else {
    for (t=0; t<items.length; t++){
      windowName=items[t].firstChild.firstChild.getAttribute('value');
      window.open('chrome://pippki/content/viewCertDetails.xul', 
                  windowName, 
                  'chrome,width=500,height=400,resizable=1');
    }
  }
}
