/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/*
  This is the old bookmarks code, included here for the sake of the bookmarks sidebar panel,
  which will be fixed to use my new code in .9. In the mean time, this file provides a
  life line to various functionality.
 */



var NC_NS  = "http://home.netscape.com/NC-rdf#";
var RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";

function Init() {
    var tree = document.getElementById("bookmarksTree");
    tree.controllers.appendController(BookmarksController);
    var children = document.getElementById('treechildren-bookmarks');
    tree.selectItem(children.firstChild);
    tree.focus();

    RefreshSort();
}

var BookmarksController = {
    supportsCommand: function(command)
    {
        switch(command)
        {
            case "cmd_undo":
            case "cmd_redo":
                return false;
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
        switch(command)
        {
            case "cmd_undo":
            case "cmd_redo":
                return false;
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
    doCommand: function(command)
    {
        switch(command)
        {
            case "cmd_undo":
            case "cmd_redo":
                break;
            case "cmd_cut":
                doCut();
                break;
            case "cmd_copy":
                doCopy();
                break;
            case "cmd_paste":
                doPaste();
                break;
            case "cmd_delete":
                doDelete();
                break;
            case "cmd_selectAll":
                doSelectAll();
                break;
        }
    },
    onEvent: function(event)
    {
        // On blur events set the menu item texts back to the normal values
        /*if (event == 'blur' )
          {
          goSetMenuValue('cmd_undo', 'valueDefault');
          goSetMenuValue('cmd_redo', 'valueDefault');
          }*/
    }
};

function CommandUpdate_Bookmarks()
{
    //goUpdateCommand('button_delete');
    // get selection info from dir pane
    /*
    var oneAddressBookSelected = false;
    if ( dirTree && dirTree.selectedItems && (dirTree.selectedItems.length == 1) )
        oneAddressBookSelected = true;

    // get selection info from results pane
    var selectedCards = GetSelectedAddresses();
    var oneOrMoreCardsSelected = false;
    if ( selectedCards )
        oneOrMoreCardsSelected = true;
    */
    // set commands to enabled / disabled
    //goSetCommandEnabled('cmd_PrintCard', oneAddressBookSelected);
    goSetCommandEnabled('bm_cmd_find', true/*oneAddressBookSelected*/);
}

function copySelectionToClipboard()
{
    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode) return false;
    var select_list = treeNode.selectedItems;
    if (!select_list) return false;
    if (select_list.length < 1) return false;

    var rdf_uri = "@mozilla.org/rdf/rdf-service;1"
    var RDF = Components.classes[rdf_uri].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    if (!RDF) return false;

    var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
    if (!Bookmarks) return false;

    var nameRes = RDF.GetResource(NC_NS + "Name");
    if (!nameRes) return false;

    // Build a url that encodes all the select nodes
    // as well as their parent nodes
    var url = "";
    var text = "";
    var html = "";

    for (var nodeIndex = 0; nodeIndex < select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (!node) continue;

        var ID = getAbsoluteID("bookmarksTree", node);
        if (!ID) continue;

        var IDRes = RDF.GetResource(ID);
        if (!IDRes) continue;
        var nameNode = Bookmarks.GetTarget(IDRes, nameRes, true);
        var theName = "";
        if (nameNode)
            nameNode =
                nameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (nameNode) theName = nameNode.Value;

        url += "ID:{" + ID + "};";
        url += "NAME:{" + theName + "};";

        if (node.getAttribute("container") == "true")
        {
            var type = node.getAttribute("type");
            if (type == NC_NS + "BookmarkSeparator")
            {
                // Note: can't encode separators in text, just html
                html += "<hr><p>";
            }
            else
            {
                text += ID + "\r";

                html += "<a href='" + ID + "'>";
                if (theName != "")
                {
                    html += theName;
                }
                html += "</a><p>";
            }
        }
    }

    if (url == "") return false;

    // get some useful components
    var trans_uri = "@mozilla.org/widget/transferable;1";
    var trans = Components.classes[trans_uri].createInstance();
    if (trans) trans = trans.QueryInterface(Components.interfaces.nsITransferable);
    if (!trans) return false;

    var clip_uri = "@mozilla.org/widget/clipboard;1";
    var clip = Components.classes[clip_uri].getService();
    if (clip) clip = clip.QueryInterface(Components.interfaces.nsIClipboard);
    if (!clip) return false;
    clip.emptyClipboard(Components.interfaces.nsIClipboard.kGlobalClipboard);

    // save bookmark's ID
    trans.addDataFlavor("moz/bookmarkclipboarditem");
    var data_uri = "@mozilla.org/supports-wstring;1";
    var data = Components.classes[data_uri].createInstance();
    if (data) {
        data = data.QueryInterface(Components.interfaces.nsISupportsWString);
    }
    if (!data) return false;
    data.data = url;
    // double byte data
    trans.setTransferData("moz/bookmarkclipboarditem", data, url.length*2);

    if (text != "")
    {
        trans.addDataFlavor("text/unicode");

        var textData_uri = "@mozilla.org/supports-wstring;1";
        var textData = Components.classes[textData_uri].createInstance();
        if (textData) textData = textData.QueryInterface(Components.interfaces.nsISupportsWString);
        if (!textData) return false;
        textData.data = text;
        // double byte data
        trans.setTransferData("text/unicode", textData, text.length*2);
    }
    if (html != "")
    {
        trans.addDataFlavor("text/html");

        var wstring_uri = "@mozilla.org/supports-wstring;1";
        var htmlData = Components.classes[wstring_uri].createInstance();
        if (htmlData) {
            var wstring_interface = Components.interfaces.nsISupportsWString;
            htmlData = htmlData.QueryInterface(wstring_interface);
        }
        if (!htmlData) return false;
        htmlData.data = html;
        // double byte data
        trans.setTransferData("text/html", htmlData, html.length*2);
    }
    clip.setData(trans, null,
                 Components.interfaces.nsIClipboard.kGlobalClipboard);
    return true;
}

function doCut()
{
    if (copySelectionToClipboard() == true) {
        doDelete(false);
    }
    return true;
}

function doCopy()
{
    copySelectionToClipboard();
    return true;
}

function doPaste()
{
    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode) return false;
    var select_list = treeNode.selectedItems;
    if (!select_list) return false;
    if (select_list.length != 1) return false;

    var pasteNodeID = select_list[0].getAttribute("id");
    var isContainerFlag = (select_list[0].getAttribute("container") == "true");

    var clip_uri = "@mozilla.org/widget/clipboard;1";
    var clip = Components.classes[clip_uri].getService();
    if (clip) clip = clip.QueryInterface(Components.interfaces.nsIClipboard);
    if (!clip) return false;

    var trans_uri = "@mozilla.org/widget/transferable;1";
    var trans = Components.classes[trans_uri].createInstance();
    if (trans) {
        trans = trans.QueryInterface(Components.interfaces.nsITransferable);
    }
    if (!trans) return false;
    trans.addDataFlavor("moz/bookmarkclipboarditem");

    clip.getData(trans, Components.interfaces.nsIClipboard.kGlobalClipboard);
    var data = new Object();
    var dataLen = new Object();
    trans.getTransferData("moz/bookmarkclipboarditem", data, dataLen);
    if (data) {
        var data_interface = Components.interfaces.nsISupportsWString
        data = data.value.QueryInterface(data_interface);
    }
    var url = null;
   // double byte data
    if (data) url = data.data.substring(0, dataLen.value / 2);
    if (!url) return false;

    var strings = url.split(";");
    if (!strings) return false;

    var rdf_uri = "@mozilla.org/rdf/rdf-service;1";
    var RDF = Components.classes[rdf_uri].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    if (!RDF) return false;
    var rdfc_uri = "@mozilla.org/rdf/container;1";
    var RDFC = Components.classes[rdfc_uri].getService();
    RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
    if (!RDFC) return false;
    var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
    if (!Bookmarks) return false;

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
    RDFC.Init(Bookmarks, pasteContainerRes);

    if (isContainerFlag == false)
    {
        pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
        if (pasteNodeIndex < 0) return false; // how did that happen?
    }

    var typeRes = RDF.GetResource(RDF_NS + "type");
    if (!typeRes) return false;
    var bmTypeRes = RDF.GetResource(NC_NS + "Bookmark");
    if (!bmTypeRes) return false;

    var dirty = false;
    for (var x=0; x<strings.length; x=x+2)
    {
        var theID = strings[x];
        var theName = strings[x+1];
        if ((theID.indexOf("ID:{") == 0) && (theName.indexOf("NAME:{") == 0))
        {
            theID = theID.substr(4, theID.length-5);
            theName = theName.substr(6, theName.length-7);

            var IDRes = RDF.GetResource(theID);
            if (!IDRes) continue;

            if (RDFC.IndexOf(IDRes) > 0)
                continue;

            if (theName != "")
            {
                var NameLiteral = RDF.GetLiteral(theName);
                if (NameLiteral)
                {
                    Bookmarks.Assert(IDRes, nameRes, NameLiteral, true);
                    dirty = true;
                }
            }
            if (isContainerFlag == true)
              RDFC.AppendElement(IDRes);
            else
              RDFC.InsertElementAt(IDRes, pasteNodeIndex++, true);
            dirty = true;

            // make sure appropriate bookmark type is set
            var bmTypeNode = Bookmarks.GetTarget( IDRes, typeRes, true );
            if (!bmTypeNode)
            {
                // set default bookmark type
                Bookmarks.Assert(IDRes, typeRes, bmTypeRes, true);
            }
        }
    }
    if (dirty == true)
    {
        var rdf_ds_interface = Components.interfaces.nsIRDFRemoteDataSource;
        var remote = Bookmarks.QueryInterface(rdf_ds_interface);
        if (remote)
          remote.Flush();
    }
    return true;
}

function doDelete(promptFlag)
{
    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode) return false;
    var select_list = treeNode.selectedItems;
    if (!select_list) return false;
    if (select_list.length < 1) return false;

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

    var RDF_uri = "@mozilla.org/rdf/rdf-service;1";
    var RDF = Components.classes[RDF_uri].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    if (!RDF) return false;

    var RDFC_uri = "@mozilla.org/rdf/container;1";
    var RDFC = Components.classes[RDFC_uri].getService();
    RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainer);
    if (!RDFC) return false;

    var Bookmarks = RDF.GetDataSource("rdf:bookmarks");
    if (!Bookmarks) return false;

    var dirty = false;

    // note: backwards delete so that we handle odd deletion cases such as
    //       deleting a child of a folder as well as the folder itself
    for (var nodeIndex=select_list.length-1; nodeIndex>=0; nodeIndex--)
    {
        var node = select_list[nodeIndex];
        if (!node) continue;
        var ID = node.getAttribute("id");
        if (!ID) continue;

        // don't allow deletion of various "special" folders
        if ((ID == "NC:BookmarksRoot") || (ID == "NC:IEFavoritesRoot"))
        {
            continue;
        }

        var parentID = node.parentNode.parentNode.getAttribute("ref");
        if (!parentID) parentID = node.parentNode.parentNode.getAttribute("id");
        if (!parentID) continue;

        var IDRes = RDF.GetResource(ID);
        if (!IDRes) continue;
        var parentIDRes = RDF.GetResource(parentID);
        if (!parentIDRes) continue;

        RDFC.Init(Bookmarks, parentIDRes);
        RDFC.RemoveElement(IDRes, true);
        dirty = true;
    }

    if (dirty == true)
    {
        var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
        if (remote)
          remote.Flush();
    }

    return true;
}

function doSelectAll()
{
    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode) return false;
    treeNode.selectAll();
    return true;
}

function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById("bookmark-window");
    win.setAttribute("x", x);
    win.setAttribute("y", y);
    win.setAttribute("height", h);
    win.setAttribute("width", w);
}

function BookmarkProperties()
{
    var treeNode = document.getElementById('bookmarksTree');
    var select_list = treeNode.selectedItems;

    if (select_list.length >= 1) {
        // don't bother showing properties on bookmark separators
        var type = select_list[0].getAttribute('type');
        if (type != NC_NS + "BookmarkSeparator") {
            window.openDialog("chrome://communicator/content/bookmarks/bm-props.xul",
                              "_blank", "centerscreen,chrome,menubar",
                              select_list[0].getAttribute("id"));
        }
    }
    return true;
}



function OpenBookmarksFind()
{
    window.openDialog("chrome://communicator/content/bookmarks/bm-find.xul",
                      "FindBookmarksWindow",
                      "dialog=no,close,chrome,resizable", "bookmarks");
    return true;
}



function getAbsoluteID(root, node)
{
    var url = node.getAttribute("ref");
    if ((url == null) || (url == ""))
    {
        url = node.getAttribute("id");
    }
    try
    {
        var rootNode = document.getElementById(root);
        var ds = null;
        if (rootNode)
        {
            ds = rootNode.database;
        }

        // add support for anonymous resources such as Internet Search results,
        // IE favorites under Win32, and NetPositive URLs under BeOS
        var rdf_uri = "@mozilla.org/rdf/rdf-service;1";
        var rdf = Components.classes[rdf_uri].getService();
        if (rdf) rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
        if (rdf && ds)
        {
            var src = rdf.GetResource(url, true);
            var prop = rdf.GetResource(NC_NS + "URL",
                                       true);
            var target = ds.GetTarget(src, prop, true);
            if (target) target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
            if (target) target = target.Value;
            if (target) url = target;
        }
    }
    catch(ex)
    {
    }
    return url;
}

function OpenURL(event, node, root)
{
    if ((event.button != 0) || (event.detail != 2)
        || (node.nodeName != "treeitem"))
        return false;

    if (node.getAttribute("container") == "true")
        return false;

    var url = getAbsoluteID(root, node);

    // Ignore "NC:" urls.
    if (url.substring(0, 3) == "NC:")
        return false;

    if (event.altKey)
    {
        BookmarkProperties();
    }
    else
    {
        // get right sized window
        window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
    }
    return true;
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function doContextCmd(cmdName)
{
    // Do some prompting/confirmation for various bookmark
    // commands that we know about.
    // If we have values to pass it, they are added to the arguments array

    var nameVal = "";
    var urlVal = "";
    var promptStr;

    var picker_uri;
    var filePicker;

    if (cmdName == NC_NS + "command?cmd=newbookmark")
    {
        while (true)
        {
            promptStr = get_localized_string("NewBookmarkURLPrompt");
            urlVal = prompt(promptStr, "");
            if (!urlVal || urlVal=="") return false;

            // ensure we get a fully qualified URL (protocol colon address)
            var colonOffset = urlVal.indexOf(":");
            if (colonOffset > 0) break;
            alert(get_localized_string("NeedValidURL"));
        }

        promptStr = get_localized_string("NewBookmarkNamePrompt");
        nameVal = prompt(promptStr, "");
        if (!nameVal || nameVal=="") return false;
    }
    else if (cmdName == NC_NS + "command?cmd=newfolder")
    {
        promptStr = get_localized_string("NewFolderNamePrompt");
        nameVal = prompt(promptStr, "");
        if (!nameVal || nameVal=="") return false;
    }
    else if ((cmdName == NC_NS + "command?cmd=deletebookmark") ||
         (cmdName == NC_NS + "command?cmd=deletebookmarkfolder") ||
         (cmdName == NC_NS + "command?cmd=deletebookmarkseparator"))
    {
        return doDelete(true);
        //var promptStr = get_localized_string("DeleteItems");
        //if (!confirm(promptStr)) return false;
    }
    else if (cmdName == NC_NS + "command?cmd=import")
    {
        try
        {
            picker_uri = "@mozilla.org/filepicker;1";
            filePicker = Components.classes[picker_uri].createInstance(nsIFilePicker);
            if (!filePicker) return false;

            promptStr = get_localized_string("SelectImport");
            filePicker.init(window, promptStr, nsIFilePicker.modeOpen);
            filePicker.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterAll);
            if (filePicker.show() != nsIFilePicker.returnCancel)
              var filename = filePicker.fileURL.spec;
            if ((!filename) || (filename == "")) return false;

            urlVal = filename;
        }
        catch(ex)
        {
            return false;
        }
    }
    else if (cmdName == NC_NS + "command?cmd=export")
    {
        try
        {
            picker_uri = "@mozilla.org/filepicker;1";
            filePicker = Components.classes[picker_uri].createInstance(nsIFilePicker);
            if (!filePicker) return false;

            promptStr = get_localized_string("EnterExport");
            filePicker.init(window, promptStr, nsIFilePicker.modeSave);
            filePicker.defaultString = "bookmarks.html";
            filePicker.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterAll);
            if (filePicker.show() != nsIFilePicker.returnCancel &&
                filePicker.fileURL.spec &&
                filePicker.fileURL.spec.length > 0) {
              urlVal = filePicker.fileURL.spec;
            } else {
              return false;
            }
        }
        catch(ex)
        {
            return false;
        }
    }

    var treeNode = document.getElementById("bookmarksTree");
    if (!treeNode) return false;
    var db = treeNode.database;
    if (!db) return false;

    var compositeDB = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
    if (!compositeDB) return false;

    var isupports_uri = "@mozilla.org/rdf/rdf-service;1";
    var isupports = Components.classes[isupports_uri].getService();
    if (!isupports) return false;
    var rdf = isupports.QueryInterface(Components.interfaces.nsIRDFService);
    if (!rdf) return false;

    // need a resource for the command
    var cmdResource = rdf.GetResource(cmdName);
    if (!cmdResource) return false;
    cmdResource = cmdResource.QueryInterface(Components.interfaces.nsIRDFResource);
    if (!cmdResource) return false;

    // set up selection nsISupportsArray
    var selection_uri = "@mozilla.org/supports-array;1";
    var selectionInstance = Components.classes[selection_uri].createInstance();
    var selectionArray = selectionInstance.QueryInterface(Components.interfaces.nsISupportsArray);

    // set up arguments nsISupportsArray
    var arguments_uri = "@mozilla.org/supports-array;1";
    var argumentsInstance = Components.classes[arguments_uri].createInstance();
    var argumentsArray = argumentsInstance.QueryInterface(Components.interfaces.nsISupportsArray);

    // get various arguments (parent, name)
    var parentArc = rdf.GetResource(NC_NS + "parent");
    if (!parentArc) return false;
    var nameArc = rdf.GetResource(NC_NS + "Name");
    if (!nameArc) return false;
    var urlArc = rdf.GetResource(NC_NS + "URL");
    if (!urlArc) return false;

    var select_list = treeNode.selectedItems;

    var uri;
    var rdfNode;

    if (select_list.length < 1)
    {
        // if nothing is selected, default to using the "ref"
        // on the root of the tree
        uri = treeNode.getAttribute("ref");
        if (!uri || uri=="") return false;
        rdfNode = rdf.GetResource(uri);
        // add node into selection array
        if (rdfNode)
        {
            selectionArray.AppendElement(rdfNode);
        }

        // add singular arguments into arguments array
        if ((nameVal) && (nameVal != ""))
        {
            var nameLiteral = rdf.GetLiteral(nameVal);
            if (!nameLiteral) return false;
            argumentsArray.AppendElement(nameArc);
            argumentsArray.AppendElement(nameLiteral);
        }
        if ((urlVal) && (urlVal != ""))
        {
            var urlLiteral = rdf.GetLiteral(urlVal);
            if (!urlLiteral) return false;
            argumentsArray.AppendElement(urlArc);
            argumentsArray.AppendElement(urlLiteral);
        }
    }
    else for (var nodeIndex=0; nodeIndex<select_list.length; nodeIndex++)
    {
        var node = select_list[nodeIndex];
        if (!node) break;
        uri = node.getAttribute("ref");
        if ((uri) || (uri == ""))
        {
            uri = node.getAttribute("id");
        }
        if (!uri) return false;

        rdfNode = rdf.GetResource(uri);
        if (!rdfNode) break;

        // add node into selection array
        selectionArray.AppendElement(rdfNode);

        // get the parent's URI
        var parentURI = "";
        var theParent = node.parentNode.parentNode;
        parentURI = theParent.getAttribute("ref");
        if ((!parentURI) || (parentURI == ""))
        {
                parentURI = theParent.getAttribute("id");
        }
        if (parentURI == "") return false;

        var parentNode = rdf.GetResource(parentURI, true);
        if (!parentNode) return false;

        // add multiple arguments into arguments array
        argumentsArray.AppendElement(parentArc);
        argumentsArray.AppendElement(parentNode);

        if ((nameVal) && (nameVal != ""))
        {
            var nameLiteral2 = rdf.GetLiteral(nameVal);
            if (!nameLiteral2) return false;
            argumentsArray.AppendElement(nameArc);
            argumentsArray.AppendElement(nameLiteral2);
        }
        if ((urlVal) && (urlVal != ""))
        {
            var urlLiteral2 = rdf.GetLiteral(urlVal);
            if (!urlLiteral2) return false;
            argumentsArray.AppendElement(urlArc);
            argumentsArray.AppendElement(urlLiteral2);
        }
    }

    // do the command
    compositeDB.DoCommand(selectionArray, cmdResource, argumentsArray);
    return true;
}

function bookmarkSelect()
{
    var tree = document.getElementById("bookmarksTree");
    var status = document.getElementById("statusbar-text");
    var val = "";
    if (tree.selectedItems.length == 1)
    {
        val = getAbsoluteID("bookmarksTree", tree.selectedItems[0]);

        // Ignore "NC:" urls.
        if (val.substring(0, 3) == "NC:")
        {
            val = "";
        }
    }
    status.label = val;
    return true;
}
