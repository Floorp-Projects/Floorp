/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl>
 *   Blake Ross <blakeross@telocity.com>
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// helper routines, for doing rdf-based cut/copy/paste/etc

const NC_NS  = "http://home.netscape.com/NC-rdf#";
const RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const nsTransferable_contractid = "@mozilla.org/widget/transferable;1";
const clipboard_contractid = "@mozilla.org/widget/clipboard;1";
const rdf_contractid = "@mozilla.org/rdf/rdf-service;1";
const SEPARATOR_URI = NC_NS + "BookmarkSeparator";
const supportswstring_contractid = "@mozilla.org/supports-wstring;1";
const rdfc_contractid = "@mozilla.org/rdf/container;1";

// some oft-used interfaces
const nsISupportsWString = Components.interfaces.nsISupportsWString;
const nsIClipboard = Components.interfaces.nsIClipboard;
const nsITransferable = Components.interfaces.nsITransferable;
const nsIRDFLiteral = Components.interfaces.nsIRDFLiteral;
const nsIRDFContainer = Components.interfaces.nsIRDFContainer;

var Clipboard = Components.classes[clipboard_contractid].getService(Components.interfaces.nsIClipboard);
var RDF = Components.classes[rdf_contractid].getService(Components.interfaces.nsIRDFService);

var nameResource = RDF.GetResource(NC_NS + "Name");
var typeRes = RDF.GetResource(RDF_NS + "type");
var bmTypeRes = RDF.GetResource(NC_NS + "Bookmark");
// this is a hack for now - just assume containment
var containment = RDF.GetResource(NC_NS + "child");

function isContainer(node)
{
    return node.getAttribute("container") == "true";
}

function getWStringData(wstring, len)
{
    wstring = wstring.QueryInterface(nsISupportsWString);
    var result = wstring.data.substring(0, len/2);
    return result;
}

function nsTreeController_SetTransferData(transferable, flavor, text)
{
    if (!text) return;
    var textData = Components.classes[supportswstring_contractid].createInstance(nsISupportsWString);
    textData.data = text;

    transferable.addDataFlavor(flavor);
    transferable.setTransferData(flavor, textData, text.length*2);
}

function nsTreeController_copy(tree)
{
    var select_list = tree.selectedItems;
    if (!select_list) return false;
    if (select_list.length < 1) return false;
    
    var datasource = tree.database;

    // Build a url that encodes all the select nodes 
    // as well as their parent nodes
    var url = "";
    var text = "";
    var html = "";

    for (var nodeIndex = 0; nodeIndex < select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (!node) continue;

        // for now just use the .id attribute.. at some point we should support
        // the #URL property for anonymous resources (see bookmarks.js)
        //var ID = getAbsoluteID("bookmarksTree", node);
        var ID = node.id;
        
        var IDRes = RDF.GetResource(ID);

        var nameNode = datasource.GetTarget(IDRes, nameResource, true);
        nameNode =
            nameNode.QueryInterface(nsIRDFLiteral);
        
        var theName = nameNode.Value;

        url += "ID:{" + ID + "};";
        url += "NAME:{" + theName + "};";

        if (isContainer(node))
        {
            // gack, need to generalize this
            var type = node.getAttribute("type");
            if (type == SEPARATOR_URI)
            {
                // Note: can't encode separators in text, just html
                html += "<hr><p>";
            }
            else
            {
                text += ID + "\r";
            
                html += "<a href='" + ID + "'>";
                if (theName) html += theName;
                html += "</a><p>";
            }
        }
    }

    if (url == "") return false;

    // get some useful components
    var trans = Components.classes[nsTransferable_contractid].createInstance(nsITransferable);

    Clipboard.emptyClipboard(nsIClipboard.kGlobalClipboard);

    this.SetTransferData(trans, "moz/bookmarkclipboarditem", url);
    this.SetTransferData(trans, "text/unicode", text);
    this.SetTransferData(trans, "text/html", html);

    Clipboard.setData(trans, null, nsIClipboard.kGlobalClipboard);
    return true;
}

function nsTreeController_cut(tree)
{
    if (this.copy(tree)) {
        this.doDelete(tree);
        return true;            // copy succeeded, don't care if delete failed
    } else
        return false;           // copy failed, so did cut
}

function nsTreeController_paste(tree)
{
    var select_list = tree.selectedItems;
    if (!select_list) return false;
    if (select_list.length != 1) return false;
    
    var datasource = tree.database;
    
    var pasteNodeID = select_list[0].id;
    var isContainerFlag = isContainer(select_list[0]);


    var trans_uri = "@mozilla.org/widget/transferable;1";
    var trans = Components.classes[nsTransferable_contractid].createInstance(nsITransferable);
    trans.addDataFlavor("moz/bookmarkclipboarditem");

    Clipboard.getData(trans, nsIClipboard.kGlobalClipboard);
    var data = new Object();
    var dataLen = new Object();
    trans.getTransferData("moz/bookmarkclipboarditem", data, dataLen);
    
    var url = getWStringData(data.value, dataLen.value);
    if (!url) return false;

    var strings = url.split(";");
    if (!strings) return false;

    var RDFC = Components.classes[rdfc_contractid].getService(nsIRDFContainer);
    
    var nameRes = RDF.GetResource(NC_NS + "Name");
    if (!nameRes) return false;

    pasteNodeRes = RDF.GetResource(pasteNodeID);
    if (!pasteNodeRes) return false;
    
    var pasteContainerRes = null;
    var pasteNodeIndex = -1;
    if (isContainerFlag == true)
    {
        pasteContainerRes = pasteNodeRes;
    }
    else
    {
        var parID = select_list[0].parentNode.parentNode.getAttribute("ref");
        if (!parID) {
            parID = select_list[0].parentNode.parentNode.getAttribute("id");
        }
        if (!parID) return false;
        pasteContainerRes = RDF.GetResource(parID);
        if (!pasteContainerRes) return false;
    }
    RDFC.Init(datasource, pasteContainerRes);

    if (isContainerFlag == false)
    {
        pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
        if (pasteNodeIndex < 0) return false; // how did that happen?
    }

    var dirty = false;
    for (var x=0; x<strings.length; x=x+2)
    {
        var theID = strings[x];
        var theName = strings[x+1];

        // make sure we have a real name/id combo
        if ((theID.indexOf("ID:{") != 0) ||
            (theName.indexOf("NAME:{") != 0))
            continue;
        
        theID = theID.substr(4, theID.length-5);
        theName = theName.substr(6, theName.length-7);
        
        var IDRes = RDF.GetResource(theID);
        if (!IDRes) continue;
        
        if (RDFC.IndexOf(IDRes) > 0)
        {
            continue;
        }
        
        // assert the name into the DS
        if (theName)
        {
            var NameLiteral = RDF.GetLiteral(theName);
            datasource.Assert(IDRes, nameRes, NameLiteral, true);
            dirty = true;
        }

        // add the element at the appropriate location
        if (isContainerFlag == true)
        {
            RDFC.AppendElement(IDRes);
        }
        else
        {
            RDFC.InsertElementAt(IDRes, pasteNodeIndex++, true);
        }
        dirty = true;

            // make sure appropriate bookmark type is set
        var bmTypeNode = datasource.GetTarget( IDRes, typeRes, true );
        if (!bmTypeNode)
        {
            // set default bookmark type
            datasource.Assert(IDRes, typeRes, bmTypeRes, true);
        }
    }
    
    if (dirty)
    {
        var remote = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
        if (remote)
        {
            remote.Flush();
        }
    }
    return true;
}

function nsTreeController_delete(tree)
{
    // this should eventually be a parameter to this function
    var promptFlag = false;
        
    if (!tree) return false;
    var select_list = tree.selectedItems;
    if (!select_list) return false;
    if (select_list.length < 1) return false;
    
    var datasource = tree.database;

    if (promptFlag == true)
    {
        var deleteStr = '';
        if (select_list.length == 1) {
            deleteStr = get_localized_string("DeleteItem");
        } else {
            deleteStr = get_localized_string("DeleteItems");
        }
        var ok = confirm(deleteStr);
        if (!ok) return false;
    }

    var RDFC = Components.classes[rdfc_contractid].getService(nsIRDFContainer);


    var dirty = false;

    // note: backwards delete so that we handle odd deletion cases such as
    //       deleting a child of a folder as well as the folder itself
    for (var nodeIndex=select_list.length-1; nodeIndex>=0; nodeIndex--)
    {
        var node = select_list[nodeIndex];
        if (!node) continue;
        var ID = node.id;
        if (!ID) continue;

        // XXX - make tree templates flag special folders as read-only
        // don't allow deletion of various "special" folders
        if ((ID == "NC:BookmarksRoot") || (ID == "NC:IEFavoritesRoot"))
        {
            continue;
        }

        var parentID = node.parentNode.parentNode.getAttribute("ref");
        if (!parentID)
            parentID = node.parentNode.parentNode.id;
        if (!parentID) continue;

        var IDRes = RDF.GetResource(ID);
        if (!IDRes) continue;
        var parentIDRes = RDF.GetResource(parentID);
        if (!parentIDRes) continue;

        try {
            // first try a container-based approach
            RDFC.Init(datasource, parentIDRes);
            RDFC.RemoveElement(IDRes, true);
        } catch (ex) {
            // nope! just remove the parent/child assertion then
            datasource.Unassert(parentIDRes, containment, IDRes);

        }
        dirty = true;
    }

    if (dirty == true)
    {
        try {
            var remote = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
            remote.Flush();
        } catch (ex) {
        }
    }

    return true;

}

function nsTreeController(tree)
{
    this.treeId = tree.id;
    tree.controllers.appendController(this);
}

nsTreeController.prototype =
{
    // store the tree's Id, rather than the tree,
    // to avoid holding a strong ref
    treeId: null,
    getTree : function()
    {
        return document.getElementById(this.treeId);
    },
    SetTransferData : nsTreeController_SetTransferData,

    supportsCommand: function(command)
    {
        switch(command)
        {
            case "cmd_cut":
            case "cmd_copy":
            case "cmd_paste":
            case "cmd_delete":
            case "cmd_selectAll":
                return true;
            default:
                return false;
        }
    },

    isCommandEnabled: function(command)
    {
        var haveCommand;
        switch (command)
        {
            // commands which do not require selection
            case "cmd_paste":
                return (this.doPaste != undefined);
                
            case "cmd_selectAll":
                // we can always select all
                return true;
                
            // these commands require selection
            case "cmd_cut":
                haveCommand = (this.cut != undefined);
                break;
            case "cmd_copy":
                haveCommand = (this.copy != undefined);
                break;
            case "cmd_delete":
                haveCommand = (this.doDelete != undefined);
                break;
        }
        
        // if we get here, then we have a command that requires selection
        var tree = this.getTree();
        var haveSelection = (tree.selectedItems.length > 0);
        return (haveCommand && haveSelection);
    },
    doCommand: function(command)
    {
        switch(command)
        {
            case "cmd_cut":
                return this.cut(this.getTree());
            
            case "cmd_copy":
                return this.copy(this.getTree());
            
            case "cmd_paste":
                return this.paste(this.getTree());

            case "cmd_delete":
                return this.doDelete(this.getTree());
            
            case "cmd_selectAll":
                return this.getTree().selectAll();
        }
        return false;
    },
    copy: nsTreeController_copy,
    cut: nsTreeController_cut,
    paste: nsTreeController_paste,
    doDelete: nsTreeController_delete
}

