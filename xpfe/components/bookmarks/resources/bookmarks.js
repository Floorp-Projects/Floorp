/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Annema <disttsc@bart.nl>
 *   Blake Ross <blakeross@telocity.com>
 */



var NC_NS  = "http://home.netscape.com/NC-rdf#";
var RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";



function debug(msg)
{
    // Uncomment for noise
    //dump(msg+"\n");
}

function Init() {
    var tree = document.getElementById("bookmarksTree");
    debug("adding controller to tree");
    tree.controllers.appendController(BookmarksController);
    var children = document.getElementById('treechildren-bookmarks');
    tree.selectItem(children.firstChild);
    tree.focus();
}

var BookmarksController = {
    supportsCommand: function(command)
    {
        debug("bookmarks in supports with " + command);
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
        debug("bookmarks in enabled with " + command);
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
        debug("bookmarks in do with " + command);
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
        debug("bookmarks in event");
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
    debug("CommandUpdate_Bookmarks()");
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
    debug("# of Nodes selected: " + select_list.length + "\n");

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

        debug("Node " + nodeIndex + ": " + ID + "    name: " + theName);
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
    debug("Copy URL: " + url);

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
    debug("Write bookmarks to clipboard");
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
    debug("Paste onto " + pasteNodeID);
    var isContainerFlag = (select_list[0].getAttribute("container") == "true");
    debug("Container status: " + ((isContainerFlag) ? "true" : "false"));

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

    debug("Inited RDFC");

    if (isContainerFlag == false)
    {
        pasteNodeIndex = RDFC.IndexOf(pasteNodeRes);
        if (pasteNodeIndex < 0) return false; // how did that happen?
    }

    var typeRes = RDF.GetResource(RDF_NS + "type");
    if (!typeRes) return false;
    var bmTypeRes = RDF.GetResource(NC_NS + "Bookmark");
    if (!bmTypeRes) return false;

    debug("Loop over strings");

    var dirty = false;
    for (var x=0; x<strings.length; x=x+2)
    {
        var theID = strings[x];
        var theName = strings[x+1];
        if ((theID.indexOf("ID:{") == 0) && (theName.indexOf("NAME:{") == 0))
        {
            theID = theID.substr(4, theID.length-5);
            theName = theName.substr(6, theName.length-7);
            debug("Paste  ID: " + theID + "    NAME: " + theName);

            var IDRes = RDF.GetResource(theID);
            if (!IDRes) continue;

            if (RDFC.IndexOf(IDRes) > 0)
            {
                debug("Unable to add ID:'" + theID +
                      "' as its already in this folder.\n");
                continue;
            }
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
            {
                RDFC.AppendElement(IDRes);
                debug("Appended node onto end of container");
            }
            else
            {
                RDFC.InsertElementAt(IDRes, pasteNodeIndex++, true);
                debug("Pasted at index # " + pasteNodeIndex);
            }
            dirty = true;

            // make sure appropriate bookmark type is set
            var bmTypeNode = Bookmarks.GetTarget( IDRes, typeRes, true );
            if (!bmTypeNode)
            {
                // set default bookmark type
                Bookmarks.Assert(IDRes, typeRes, bmTypeRes, true);
                debug("Setting default bookmark type\n");
            }
        }
    }
    if (dirty == true)
    {
        var rdf_ds_interface = Components.interfaces.nsIRDFRemoteDataSource;
        var remote = Bookmarks.QueryInterface(rdf_ds_interface);
        if (remote)
        {
            remote.Flush();
            debug("Wrote out bookmark changes.");
        }
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

    debug("# of Nodes selected: " + select_list.length);

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

        debug("Node " + nodeIndex + ": " + ID);
        debug("Parent Node " + nodeIndex + ": " + parentID);

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
        {
            remote.Flush();
            debug("Wrote out bookmark changes.");
        }
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
                      "centerscreen,dialog=no,close,chrome,resizable");
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
    if ((event.button != 1) || (event.detail != 2)
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


function update_sort_menuitems(column, direction)
{
    var unsorted_menuitem = document.getElementById("unsorted_menuitem");
    var sort_ascending = document.getElementById('sort_ascending');
    var sort_descending = document.getElementById('sort_descending');

    // as this function may be called from various places, including the
    // bookmarks sidebar panel (which doesn't have any menu items)
    // ensure that the document contains the elements
    if ((!unsorted_menuitem) || (!sort_ascending) || (!sort_descending))
        return;

    if (direction == "natural") {
        unsorted_menuitem.setAttribute('checked','true');
        sort_ascending.setAttribute('disabled','true');
        sort_descending.setAttribute('disabled','true');
        sort_ascending.removeAttribute('checked');
        sort_descending.removeAttribute('checked');
    } else {
        sort_ascending.removeAttribute('disabled');
        sort_descending.removeAttribute('disabled');
        if (direction == "ascending") {
            sort_ascending.setAttribute('checked','true');
        } else {
            sort_descending.setAttribute('checked','true');
        }

        var columns = document.getElementById('theColumns');
        var column_node = columns.firstChild;
        var column_name = column.id;
        var menuitem = document.getElementById('fill_after_this_node');
        menuitem = menuitem.nextSibling
        while (1) {
            var name = menuitem.getAttribute('column_id');
            debug("update: "+name)
            if (!name) break;
            if (column_name == name) {
                menuitem.setAttribute('checked', 'true');
                break;
            }
            menuitem = menuitem.nextSibling;
            column_node = column_node.nextSibling;
            if (column_node && column_node.tagName == "splitter")
                column_node = column_node.nextSibling;
        }
    }
    enable_sort_menuitems();
}

function enable_sort_menuitems() {
    var columns = document.getElementById('theColumns');
    var column_node = columns.firstChild;
    var head = document.getElementById('headRow');
    var tree_column = head.firstChild;
    var skip_column = document.getElementById('popupCell');
    var menuitem = document.getElementById('fill_after_this_node');
    menuitem = menuitem.nextSibling
    while (column_node) {
        if (skip_column != tree_column) {
            if ("true" == column_node.getAttribute("hidden")) {
                menuitem.setAttribute("disabled", "true");
            } else {
                menuitem.removeAttribute("disabled");
            }
        }
        menuitem = menuitem.nextSibling;
        tree_column = tree_column.nextSibling;
        column_node = column_node.nextSibling;

        if (column_node && column_node.tagName == "splitter")
            column_node = column_node.nextSibling;
    }
}

function find_sort_column() {
    var columns = document.getElementById('theColumns');
    var column = columns.firstChild;
    while (column) {
        if ("true" == column.getAttribute('sortActive')) {
            return column;
        }
        column = column.nextSibling;
    }
    return columns.firstChild;
}

function find_sort_direction(column) {
    if ("true" == column.getAttribute('sortActive')) {
        return column.getAttribute('sortDirection');
    } else {
        return "natural";
    }
}

function SetSortDirection(direction)
{
    debug("SetSortDirection("+direction+")");
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    if (current_direction != direction) {
        sort_column(current_column, direction);
    }
}

function SetSortColumn(column_name)
{
    debug("SetSortColumn("+column_name+")");
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    var column = document.getElementById(column_name);
    if (column != current_column || current_direction == "natural") {
        sort_column(column, "ascending");
    }
}

function TriStateColumnSort(column_name)
{
    debug("TriStateColumnSort("+column_name+")");
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    var column = document.getElementById(column_name);
    if (!column) return false;
    var direction = "ascending";
    if (column == current_column) {
        if (current_direction == "ascending") {
            direction = "descending";
        } else if (current_direction == "descending") {
            direction = "natural";
        }
    }
    sort_column(column, direction);
    return true;
}

function sort_column(column, direction)
{
    var isupports_uri = "@mozilla.org/rdf/xul-sort-service;1";
    var isupports = Components.classes[isupports_uri].getService();
    if (!isupports) return false;
    var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
    if (!xulSortService) return false;
    try
    {
        var sort_resource = column.getAttribute('resource');
        xulSortService.Sort(column, sort_resource, direction);
    }
    catch(ex)
    {
        debug("Exception calling xulSortService.Sort()");
    }
    update_sort_menuitems(column, direction);
    return false;
}

function fillViewMenu(popup)
{
  var fill_after = document.getElementById('fill_after_this_node');
  var fill_before = document.getElementById('fill_before_this_node');
  var columns = document.getElementById('theColumns');
  var head = document.getElementById('headRow');
  var skip_column = document.getElementById('popupCell');
      
  if (fill_after.nextSibling == fill_before) {
      var name_template = get_localized_string("SortMenuItem");
      var tree_column = head.firstChild;
      var column_node = columns.firstChild;
      while (tree_column) {
          if (skip_column != tree_column) {
              // Construct an entry for each cell in the row.
              var column_name = tree_column.getAttribute("value");
              var item = document.createElement("menuitem");
              item.setAttribute("type", "radio");
              item.setAttribute("name", "sort_column");
              if (column_name == "") {
                  column_name = tree_column.getAttribute("display");
              }
              var name = name_template.replace(/%NAME%/g, column_name);
              var id = column_node.id;
              item.setAttribute("value", name);
              item.setAttribute("oncommand", "SetSortColumn('"+id+"', true);");
              item.setAttribute("column_id", id);
              
              popup.insertBefore(item, fill_before);
          }
          tree_column = tree_column.nextSibling;
          column_node = column_node.nextSibling;

          if (column_node && column_node.tagName == "splitter")
              column_node = column_node.nextSibling;
      }
  }
  var sort_column = find_sort_column();
  var sort_direction = find_sort_direction(sort_column);
  update_sort_menuitems(sort_column, sort_direction);
}

function fillContextMenu(name)
{
    if (!name) return false;
    var popupNode = document.getElementById(name);
    if (!popupNode) return false;
    // remove the menu node (which tosses all of its kids);
    // do this in case any old command nodes are hanging around
    while (popupNode.childNodes.length)
    {
      popupNode.removeChild(popupNode.childNodes[0]);
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

    var target_item = document.popupNode.parentNode.parentNode;
    if (target_item && target_item.nodeName == "treeitem")
    {
        if (target_item.getAttribute('selected') != 'true') {
          treeNode.selectItem(target_item);
        }
    }

    var select_list = treeNode.selectedItems;
    debug("# of Nodes selected: " + treeNode.selectedItems.length + "\n");

    var separatorResource =
        rdf.GetResource(NC_NS + "BookmarkSeparator");
    if (!separatorResource) return false;

    // perform intersection of commands over selected nodes
    var cmdArray = new Array();
    var selectLength = select_list.length;
    if (selectLength == 0) selectLength = 1;
    for (var nodeIndex=0; nodeIndex < selectLength; nodeIndex++)
    {
        var id = null;

        // If nothing is selected, get commands for the "ref"
        // of the bookmarks root
        if (select_list.length == 0)
        {
            id = treeNode.getAttribute("ref");
            if (!id) break;
        }
        else
        {
            var node = select_list[nodeIndex];
            if (!node) break;
            id = node.getAttribute("id");
            if (!id) break;
        }
        var rdfNode = rdf.GetResource(id);
        if (!rdfNode) break;
        var cmdEnum = db.GetAllCmds(rdfNode);
        if (!cmdEnum) break;

        var nextCmdArray = new Array();
        while (cmdEnum.hasMoreElements())
        {
            var cmd = cmdEnum.getNext();
            if (!cmd) break;

            if (nodeIndex == 0)
            {
                cmdArray[cmdArray.length] = cmd;
            }
            else
            {
                nextCmdArray[cmdArray.length] = cmd;
            }
        }
        if (nodeIndex > 0)
        {
            // perform command intersection calculation
            for (var cmdIndex = 0; cmdIndex < cmdArray.length; cmdIndex++)
            {
                var cmdFound = false;
                for (var nextCmdIndex = 0;
                     nextCmdIndex < nextCmdArray.length;
                     nextCmdIndex++)
                {
                    if (nextCmdArray[nextCmdIndex] == cmdArray[cmdIndex])
                    {
                        cmdFound = true;
                        break;
                    }
                }
                if ((cmdFound == false) && (cmdArray[cmdIndex]))
                {
                    var cmdResource = cmdArray[cmdIndex].QueryInterface(Components.interfaces.nsIRDFResource);
                    if ((cmdResource) && (cmdResource != separatorResource))
                    {
                        cmdArray[cmdIndex] = null;
                    }
                }
            }
        }
    }

    // need a resource to ask RDF for each command's name
    var rdfNameResource = rdf.GetResource(NC_NS + "Name");
    if (!rdfNameResource) return false;

/*
    // build up menu items
    if (cmdArray.length < 1) return false;
*/

    var lastWasSep = false;
    for (var commandIndex = 0; commandIndex < cmdArray.length; commandIndex++)
    {
        var cmd = cmdArray[commandIndex];
        if (!cmd) continue;
        var cmdResource = cmd.QueryInterface(Components.interfaces.nsIRDFResource);
        if (!cmdResource) break;

        // handle separators
        if (cmdResource == separatorResource)
        {
            if (lastWasSep != true)
            {
                lastWasSep = true;
                    var menuItem = document.createElement("menuseparator");
                    popupNode.appendChild(menuItem);
            }
            continue;
        }
    
        lastWasSep = false;

        var cmdNameNode = compositeDB.GetTarget(cmdResource, rdfNameResource, 
                                                true);
        if (!cmdNameNode) break;
        cmdNameLiteral = cmdNameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (!cmdNameLiteral) break;
        cmdName = cmdNameLiteral.Value;
        if (!cmdName) break;

        debug("Command #" + cmdIndex + ": id='" + cmdResource.Value +
              "'  name='" + cmdName + "'");

        var newMenuItem = document.createElement("menuitem");
        newMenuItem.setAttribute("value", cmdName);
        popupNode.appendChild(newMenuItem);
        // Work around bug #26402 by setting "oncommand" attribute
        // AFTER appending menuitem
        newMenuItem.setAttribute("oncommand",
                              "return doContextCmd('"+cmdResource.Value+"');");
    }

    // strip off leading/trailing menuseparators
    while (popupNode.childNodes.length > 0)
    {
        if (popupNode.childNodes[0].tagName != "menuseparator")
            break;
        popupNode.removeChild(popupNode.childNodes[0]);
    }
    while (popupNode.childNodes.length > 0)
    {
        if (popupNode.childNodes[popupNode.childNodes.length - 1].tagName != "menuseparator")
            break;
        popupNode.removeChild(popupNode.childNodes[popupNode.childNodes.length - 1]);
    }

    // if one and only one node is selected
    if (treeNode.selectedItems.length == 1)
    {
        // and its a bookmark or a bookmark folder (there can be other types,
        // not just separators, so check explicitly for what we allow)
        var type = select_list[0].getAttribute("type");
        if ((type == NC_NS + "Bookmark") ||
            (type == NC_NS + "Folder"))
        {
            // then add a menu separator (if necessary)
            if (popupNode.childNodes.length > 0)
            {
                if (popupNode.childNodes[popupNode.childNodes.length - 1].tagName != "menuseparator")
                {
                    var menuSep = document.createElement("menuseparator");
                    popupNode.appendChild(menuSep);
                }
            }

            // And then add a "Properties" menu items
            var propMenuName = get_localized_string("BookmarkProperties");
            var aMenuItem = document.createElement("menuitem");
            aMenuItem.setAttribute("value", propMenuName);
            popupNode.appendChild(aMenuItem);
            // Work around bug # 26402 by setting "oncommand" attribute
            // AFTER appending menuitem
            aMenuItem.setAttribute("oncommand", "return BookmarkProperties();");
        }
    }

    return true;
}


const nsIFilePicker = Components.interfaces.nsIFilePicker;

function doContextCmd(cmdName)
{
    debug("doContextCmd start: cmd='" + cmdName + "'");

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

            debug("Import: '" + filename + "'\n");
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
              debug("Export: '" + urlVal + "'\n");
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
    debug("# of Nodes selected: " + select_list.length);

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

    debug("doContextCmd ends.");
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
    status.setAttribute("value", val);
    return true;
}

function get_localized_string(name) {
    var uri = "chrome://communicator/locale/bookmarks/bookmark.properties";
    var bundle = srGetStrBundle(uri);
    return bundle.GetStringFromName(name);
}

//*==================================================
// Handy debug routines
//==================================================
function dump_attributes(node,depth) {
  var attributes = node.attributes;
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | . ";

  if (!attributes || attributes.length == 0) {
    debug(indent.substr(indent.length - depth*2) + "no attributes");
  }
  for (var ii=0; ii < attributes.length; ii++) {
    var attr = attributes.item(ii);
    debug(indent.substr(indent.length - depth*2) + attr.name +"="+attr.value);
  }
}

function dump_tree(node) {
  dump_tree_recur(node, 0, 0);
}

function dump_tree_recur(node, depth, index) {
  if (!node) {
    debug("dump_tree: node is null");
  }
  var indent = "| | | | | | | | | | | | | | | | | | | | | | | | | | | | | + ";
  debug(indent.substr(indent.length - depth*2) + index + " " + node.nodeName);
  if (node.nodeName != "#text") {
    //debug(" id="+node.getAttribute('id'));
    dump_attributes(node, depth);
  }
  var kids = node.childNodes;
  for (var ii=0; ii < kids.length; ii++) {
    dump_tree_recur(kids[ii], depth + 1, ii);
  }
}
//==================================================
// end of handy debug routines
//==================================================*/
