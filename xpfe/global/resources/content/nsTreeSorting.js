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


// utility routines for sorting

// re-does a sort based on the current state
function RefreshSort()
{
  var current_column = find_sort_column();
  SortColumn(current_column.id);
}

// set the sort direction on the currently sorted column
function SortInNewDirection(direction)
{
  var current_column = find_sort_column();
  if (direction == "ascending")
    direction = "natural";
  else if (direction == "descending")
    direction = "ascending";
  else if (direction == "natural")
    direction = "descending";
  current_column.setAttribute("sortDirection", direction);
  SortColumn(current_column.id);
}

function SortColumn(columnID)
{
  var column = document.getElementById(columnID);
  column.parentNode.outlinerBoxObject.view.cycleHeader(columnID, column);
}

// search over the columns to find the first one with an active sort
function find_sort_column()
{
  var columns = document.getElementsByTagName('outlinercol');
  for (var i = 0; i < columns.length; ++i) {
    if (columns[i].getAttribute('sortDirection'))
      return columns[i];
  }
  return columns[0];
}

// get the sort direction for the given column
function find_sort_direction(column)
{
  var sortDirection = column.getAttribute('sortDirection');
  return (sortDirection ? sortDirection : "natural");
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

    var columns = document.getElementsByTagName('outlinercol');
    var i = 0;
    var column_node = columns[i];
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
      column_node = columns[++i];
      if (column_node && column_node.tagName == "splitter") {
        column_node = columns[++i];
      }
    }
  }
  enable_sort_menuitems();
}

function enable_sort_menuitems()
{
  var columns = document.getElementsByTagName('outlinercol');
  var i = 0;
  var column_node = columns[i];
  var menuitem = document.getElementById('fill_after_this_node');
  menuitem = menuitem.nextSibling
  while (column_node && menuitem) {
    if (column_node.getAttribute("hidden") == "true")
      menuitem.setAttribute("disabled", "true");
    else
      menuitem.removeAttribute("disabled");
    menuitem = menuitem.nextSibling;
    column_node = columns[++i];
  }
}

function fillViewMenu(popup)
{
  var fill_after = document.getElementById('fill_after_this_node');
  var fill_before = document.getElementById('fill_before_this_node');
  var columns = document.getElementsByTagName('outlinercol');
  var strBundle = document.getElementById('sortBundle');
  var sortString;
  if (strBundle)
    sortString = strBundle.getString('SortMenuItems');
  if (!sortString)
    sortString = "Sorted by %COLNAME%";
    
  var i = 0;
  var column = columns[i];
  var popupChild = popup.firstChild.nextSibling.nextSibling;
  var firstTime = (fill_after.nextSibling == fill_before);
  while (column) {
    if (firstTime) {
      // Construct an entry for each cell in the row.
      var column_name = column.getAttribute("label");
      var item = document.createElement("menuitem");
      item.setAttribute("type", "radio");
      item.setAttribute("name", "sort_column");
      if (column_name == "")
        column_name = column.getAttribute("display");
      var name = sortString.replace(/%COLNAME%/g, column_name);
      item.setAttribute("label", name);
      item.setAttribute("oncommand", "SortColumn('" + column.id + "');");
      item.setAttribute("column_id", column.id);
      popup.insertBefore(item, fill_before);
    }
    column = columns[++i];
  }
  var sort_column = find_sort_column();
  var sort_direction = find_sort_direction(sort_column);
  update_sort_menuitems(sort_column, sort_direction);
}
