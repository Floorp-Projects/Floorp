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
 *  David Drinan <ddrinan@netscape.com>
 */

const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsICrlEntry = Components.interfaces.nsICrlEntry;
const nsISupportsArray = Components.interfaces.nsISupportsArray;

var certdb;
var crls;

function onLoad()
{
  var crlEntry;
  var i;

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  crls = certdb.getCrls();

  for (i=0; i<crls.Count(); i++) {
    crlEntry = crls.GetElementAt(i).QueryInterface(nsICrlEntry);
    var name = crlEntry.name;
    var lastUpdate = crlEntry.lastUpdate;
    var nextUpdate = crlEntry.nextUpdate;
    AddItem("crlList", [name, lastUpdate, nextUpdate], "crltree_", i);
  }
}

function AddItem(children,cells,prefix,idfier)
{
  var kids = document.getElementById(children);
  var item  = document.createElement("treeitem");
  var row   = document.createElement("treerow");
  for(var i = 0; i < cells.length; i++)
  {
    var cell  = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("label", cells[i])
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

function DeleteCrlSelected() {
  var crlEntry;

  // delete selected item
  var crltree = document.getElementById("crltree");
  var i = crltree.selectedIndex;

  // Delete it
  certdb.deleteCrl(i);
  DeleteItemSelected("crltree", "crltree_", "crlList");
  if( !crltree.selectedItems.length ) {
    if( !document.getElementById("deleteCrl").disabled ) {
      document.getElementById("deleteCrl").setAttribute("disabled", "true")
    }
  }
}

function EnableCrlActions() {
  document.getElementById("deleteCrl").removeAttribute("disabled", "true");
  // document.getElementById("updateCrl").removeAttribute("disabled", "true");
}

function DeleteItemSelected(tree, prefix, kids) {
  var i;
  var delnarray = [];
  var rv = "";
  var cookietree = document.getElementById(tree);
  var selitems = cookietree.selectedItems;
  for(i = 0; i < selitems.length; i++) 
  { 
    delnarray[i] = document.getElementById(selitems[i].getAttribute("id"));
    var itemid = parseInt(selitems[i].getAttribute("id").substring(prefix.length,selitems[i].getAttribute("id").length));
    rv += (itemid + ",");
  }
  for(i = 0; i < delnarray.length; i++) 
  { 
    document.getElementById(kids).removeChild(delnarray[i]);
  }
  return rv;
}
