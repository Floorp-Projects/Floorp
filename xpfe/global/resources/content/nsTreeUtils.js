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


// utility routines for things like sorting and menus
// eventually this should probably be broken up even further

function TriStateColumnSort(column_name)
{
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


// Note: doSort() does NOT support natural order sorting, unless naturalOrderResource is valid,
//       in which case we sort ascending on naturalOrderResource

function doSort(sortColName, naturalOrderResource)
{
    var node = document.getElementById(sortColName);
    // determine column resource to sort on
    var sortResource = node.getAttribute('resource');
    if (!sortResource)
        return false;

    var sortDirection="ascending";
    var isSortActive = node.getAttribute('sortActive');
    if (isSortActive == "true") {
        sortDirection = "ascending";
        var currentDirection = node.getAttribute('sortDirection');
        if (currentDirection == "ascending") {
            if (sortResource != naturalOrderResource)
                sortDirection = "descending";
        }
        else if (currentDirection == "descending") {
            if (naturalOrderResource)
                sortResource = naturalOrderResource;
        }
    }

    try {
        var isupports = Components.classes["@mozilla.org/xul/xul-sort-service;1"].getService();
        var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
    }
    catch(ex) {
        dump(ex);
        return false;
    }

    try {
        xulSortService.Sort(node, sortResource, sortDirection);
    }
    catch(ex) {
    }
    return true;
}

// re-does a sort based on the current state
function RefreshSort()
{
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    sort_column(current_column, current_direction);
}

// set the sort direction on the currently sorted column
function SetSortDirection(direction)
{
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    if (current_direction != direction) {
        sort_column(current_column, direction);
    }
}

// set the sorted column
function SetSortColumn(column_name)
{
    var current_column = find_sort_column();
    var current_direction = find_sort_direction(current_column);
    var column = document.getElementById(column_name);
    if (column != current_column || current_direction == "natural") {
        sort_column(column, "ascending");
    }
}

// actually sort given the column and direction
function sort_column(column, direction)
{
    var isupports_uri = "@mozilla.org/xul/xul-sort-service;1";
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
        dump("sort exception: " + ex + "\n");
    }
    update_sort_menuitems(column, direction);
    return false;
}

// search over the columns to find the first one with an active sort
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


// get the sort direction for the given column
function find_sort_direction(column) {
    if ("true" == column.getAttribute('sortActive')) {
        return column.getAttribute('sortDirection');
    } else {
        return "natural";
    }
}

// set up the menu items to reflect the specified sort column
// and direction - put check marks next to the active ones, and clear
// out the old ones
// - disable ascending/descending direction if the tree isn't sorted
// - disable columns that are not visible
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
    while (column_node && menuitem) {
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


function fillViewMenu(popup)
{
  var fill_after = document.getElementById('fill_after_this_node');
  var fill_before = document.getElementById('fill_before_this_node');
  var columns = document.getElementById('theColumns');
  var head = document.getElementById('headRow');
  var skip_column = document.getElementById('popupCell');

  var name_template = get_localized_string("SortMenuItem");
  var tree_column = head.firstChild;
  var column_node = columns.firstChild;
  var popupChild = popup.firstChild.nextSibling.nextSibling;
  var firstTime = (fill_after.nextSibling == fill_before);
  while (tree_column) {
      if (firstTime) {
          if (skip_column != tree_column && tree_column.getAttribute("collapsed") != "true") {
              // Construct an entry for each cell in the row.
              var column_name = tree_column.getAttribute("label");
              var item = document.createElement("menuitem");
              item.setAttribute("type", "radio");
              item.setAttribute("name", "sort_column");
              if (column_name == "")
                  column_name = tree_column.getAttribute("display");
              var name = name_template.replace(/%NAME%/g, column_name);
              var id = column_node.id;
              item.setAttribute("label", name);
              item.setAttribute("oncommand", "SetSortColumn('"+id+"', true);");
              item.setAttribute("column_id", id);

              popup.insertBefore(item, fill_before);
          }
      }
      else {
          if (column_node.getAttribute("collapsed") == "true")
              popupChild.setAttribute("hidden", "true");
          else
              popupChild.removeAttribute("hidden");
          popupChild = popupChild.nextSibling;
      }
      tree_column = tree_column.nextSibling;
      column_node = column_node.nextSibling;
      if (column_node && column_node.tagName == "splitter")
          column_node = column_node.nextSibling;
  }
  var sort_column = find_sort_column();
  var sort_direction = find_sort_direction(sort_column);
  update_sort_menuitems(sort_column, sort_direction);
}

function fillContextMenu(name, treeName)
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

    var treeNode = document.getElementById(treeName);
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
        cmd = cmdArray[commandIndex];
        if (!cmd) continue;
        cmdResource = cmd.QueryInterface(Components.interfaces.nsIRDFResource);
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
        var cmdNameLiteral = cmdNameNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
        if (!cmdNameLiteral) break;
        var cmdName = cmdNameLiteral.Value;
        if (!cmdName) break;

        var newMenuItem = document.createElement("menuitem");
        newMenuItem.setAttribute("label", cmdName);
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
            aMenuItem.setAttribute("label", propMenuName);
            popupNode.appendChild(aMenuItem);
            // Work around bug # 26402 by setting "oncommand" attribute
            // AFTER appending menuitem
            aMenuItem.setAttribute("oncommand", "return BookmarkProperties();");
        }
    }

    return true;
}

// this is really unnecessary, we should be using <stringbundle>
function get_localized_string(name) {
    var uri = "chrome://communicator/locale/bookmarks/bookmark.properties";
    var bundle = srGetStrBundle(uri);
    return bundle.GetStringFromName(name);
}
