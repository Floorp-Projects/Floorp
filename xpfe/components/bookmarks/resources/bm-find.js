/* -*- Mode: Java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Original Author(s):
 *    Robert John Churchill    <rjc@netscape.com>
 *
 * Contributor(s): 
 *    Blake Ross <blakeross@telocity.com>
 *    Peter Annema <disttsc@bart.nl>
 */

var findText;

function debug(msg)
{
	// uncomment for noise
	// dump(msg+"\n");
}



function doLoad()
{
	findText = document.getElementById("findtext");

	// disable "Save Search" button (initially)
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.setAttribute("disabled", "true");
	}

	doEnabling();

	// set initial focus
	findText.focus();
}



function doUnload()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "bookmark-find-window" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
}




var	gDatasourceName = "";
var	gMatchName = "";
var	gMethodName = "";
var	gTextName = "";

function doFind()
{
	gDatasourceName = "";
	gMatchName = "";
	gMethodName = "";
	gTextName = "";

	// get RDF datasource to query
	var datasourceNode = document.getElementById("datasource");
	if (!datasourceNode)	return(false);
	var datasource = datasourceNode.selectedItem.getAttribute("data");
	gDatasourceName = datasourceNode.selectedItem.getAttribute("value");
	debug("Datasource: " + gDatasourceName + "\n");

	// get match
	var matchNode = document.getElementById("match");
	if (!matchNode)		return(false);
	var match = matchNode.selectedItem.getAttribute("data");
	gMatchName = matchNode.selectedItem.getAttribute("value");
	debug("Match: " + match + "\n");

	// get method
	var methodNode = document.getElementById("method");
	if (!methodNode)	return(false);
	var method = methodNode.selectedItem.getAttribute("data");
	gMethodName = methodNode.selectedItem.getAttribute("value");
	debug("Method: " + method + "\n");

	// get user text to find
	if (!findText)	return(false);
	gTextName = findText.value;
	if (!gTextName || gTextName=="")	return(false);
	debug("Find text: " + gTextName + "\n");

	// construct find URL
	var url = "find:datasource=" + datasource;
	url += "&match=" + match;
	url += "&method=" + method;
	url += "&text=" + escape(gTextName);
	debug("Find URL: " + url + "\n");

	// load find URL into results pane
	var resultsTree = document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
        resultsTree.setAttribute("ref", "");
        resultsTree.setAttribute("ref", url);

	// enable "Save Search" button
	var searchButton = document.getElementById("SaveSearch");
	if (searchButton)
	{
		searchButton.removeAttribute("disabled", "true");
	}

	debug("doFind done.\n");
	return(true);
}



function saveFind()
{
	var resultsTree = document.getElementById("findresultstree");
	if (!resultsTree)	return(false);
	var searchURL = resultsTree.getAttribute("ref");
	if ((!searchURL) || (searchURL == ""))		return(false);

	debug("Bookmark search URL: " + searchURL + "\n");
	var searchTitle = "Find: " + gMatchName + " " + gMethodName + " '" + gTextName + "' in " + gDatasourceName;
	debug("Title: " + searchTitle + "\n\n");

	var bmks = Components.classes["@mozilla.org/browser/bookmarks-service;1"].getService();
	if (bmks)	bmks = bmks.QueryInterface(Components.interfaces.nsIBookmarksService);
	if (bmks)	bmks.AddBookmark(searchURL, searchTitle, bmks.BOOKMARK_FIND_TYPE, null);

	return(true);
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
		var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
		if (rdf)   rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
		if (rdf && ds)
		{
			var src = rdf.GetResource(url, true);
			var prop = rdf.GetResource("http://home.netscape.com/NC-rdf#URL", true);
			var target = ds.GetTarget(src, prop, true);
			if (target)	target = target.QueryInterface(Components.interfaces.nsIRDFLiteral);
			if (target)	target = target.Value;
			if (target)	url = target;
		}
	}
	catch(ex)
	{
	}
	return(url);
}



function OpenURL(event, node, root)
{
	if ((event.button != 1) || (event.detail != 2) || (node.nodeName != "treeitem"))
		return(false);

	if (node.getAttribute("container") == "true")
		return(false);

	var url = getAbsoluteID(root, node);

	// Ignore "NC:" urls.
	if (url.substring(0, 3) == "NC:")
		return(false);

	// get right sized window
	window.openDialog( getBrowserURL(), "_blank", "chrome,all,dialog=no", url );
	return(true);
}



/* Note: doSort() does NOT support natural order sorting, unless naturalOrderResource is valid,
         in which case we sort ascending on naturalOrderResource
 */
function doSort(sortColName, naturalOrderResource)
{
	var node = document.getElementById(sortColName);
	// determine column resource to sort on
	var sortResource = node.getAttribute('resource');
	if (!sortResource) return(false);

	var sortDirection="ascending";
	var isSortActive = node.getAttribute('sortActive');
	if (isSortActive == "true")
	{
		sortDirection = "ascending";

		var currentDirection = node.getAttribute('sortDirection');
		if (currentDirection == "ascending")
		{
			if (sortResource != naturalOrderResource)
			{
				sortDirection = "descending";
			}
		}
		else if (currentDirection == "descending")
		{
			if (naturalOrderResource != null && naturalOrderResource != "")
			{
				sortResource = naturalOrderResource;
			}
		}
	}

	var isupports = Components.classes["@mozilla.org/rdf/xul-sort-service;1"].getService();
	if (!isupports)    return(false);
	var xulSortService = isupports.QueryInterface(Components.interfaces.nsIXULSortService);
	if (!xulSortService)    return(false);
	try
	{
		xulSortService.Sort(node, sortResource, sortDirection);
	}
	catch(ex)
	{
		debug("Exception calling xulSortService.Sort()");
	}
	return(true);
}



function itemSelect()
{
	var tree = document.getElementById("findresultstree");
	if (tree.selectedItems.length == 1)
	{
		var status = document.getElementById("statusbar-display");
		var val = getAbsoluteID("findresultstree", tree.selectedItems[0]);

		// Ignore "NC:" urls.
		if (val.substring(0, 3) == "NC:") 
			status.value = "";
		else
			status.value = val;
	}
}



function doEnabling()
{
	var findButton = document.getElementById("search");

	if (!findText.value) {
		// No input, disable find button if enabled.
		if (!findButton.disabled) {
			findButton.disabled = true;
		}
	}
	else
		if (findButton.disabled)
			findButton.disabled = false;
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



function sort_column(column, direction)
{
    var isupports_uri = "component://netscape/rdf/xul-sort-service";
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



function get_localized_string(name) {
    var uri = "chrome://communicator/locale/bookmarks/bookmark.properties";
    var bundle = srGetStrBundle(uri);
    return bundle.GetStringFromName(name);
}

//*==================================================
