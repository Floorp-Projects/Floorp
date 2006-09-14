/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Jason Eager <jce2@po.cwru.edu>
 *  Blake Ross <BlakeR1234@aol.com>
 *  Peter Annema <disttsc@bart.nl>
 *
 */
const MAX_HISTORY_MENU_ITEMS = 15;
const MAX_HISTORY_ITEMS = 100;
var rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"]
             .getService(Components.interfaces.nsIRDFService);
var rdfc = Components.classes["@mozilla.org/rdf/container-utils;1"]
             .getService(Components.interfaces.nsIRDFContainerUtils);
var localstore = rdf.GetDataSource("rdf:localstore");

function FillHistoryMenu( aParent, aMenu )
  {
    // Get the content area docshell
    var browserElement = document.getElementById("content");
    if (browserElement)
      {
        var webNavigation = browserElement.webNavigation;
        if (webNavigation)
          {
            var shistory = webNavigation.sessionHistory;
            if (shistory)
              {
                //Remove old entries if any
                deleteHistoryItems( aParent );
                var count = shistory.count;
                var index = shistory.index;
                var end;
                var j;
                var entry;

                switch (aMenu)
                  {
                    case "back":
                      end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
                      for ( j = index - 1; j >= end; j--)
                        {
                          entry = shistory.getEntryAtIndex(j, false);
                          if (entry)
                            createMenuItem( aParent, j, entry.title );
                        }
                      break;
                    case "forward":
                      end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
                      for ( j = index + 1; j < end; j++)
                        {
                          entry = shistory.getEntryAtIndex(j, false);
                          if (entry)
                            createMenuItem( aParent, j, entry.title );
                        }
                      break;
                    case "go":
                      end = count > MAX_HISTORY_MENU_ITEMS ? count - MAX_HISTORY_MENU_ITEMS : 0;
                      for( j = count - 1; j >= end; j-- )
                        {
                          entry = shistory.getEntryAtIndex(j, false);
                          if (entry)
                            createCheckboxMenuItem( aParent, j, entry.title, j==index );
                        }
                      break;
                  }
              }
          }
      }
  }

function executeUrlBarHistoryCommand( aTarget )
  {
    var index = aTarget.getAttribute("index");
    var value = aTarget.getAttribute("value");
    if (index != "nothing_available" && value)
      {
        gURLBar.value = value;
        BrowserLoadURL();
      }
  }

function createUBHistoryMenu( aParent )
  {
    if (localstore) {
      var entries = rdfc.MakeSeq(localstore, rdf.GetResource("nc:urlbar-history")).GetElements();
      var i= MAX_HISTORY_MENU_ITEMS;

      // Delete any old menu items only if there are legitimate
      // urls to display, otherwise we want to display the
      // '(Nothing Available)' item.
      deleteHistoryItems(aParent);
      if (!entries.hasMoreElements()) {
        //Create the "Nothing Available" Menu item
        var na = bundle.GetStringFromName( "nothingAvailable" );
        createMenuItem(aParent, "nothing_available", na);
      }

      while (entries.hasMoreElements() && (i-- > 0)) {
        var entry = entries.getNext();
        if (entry) {
          entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
          var url = entry.Value;
          createMenuItem(aParent, i, url);
        }
      }
    }
  }

function addToUrlbarHistory()
  {
	var urlToAdd = gURLBar.value;
	if (!urlToAdd)
	   return;
	if (localstore) {
    var entries = rdfc.MakeSeq(localstore, rdf.GetResource("nc:urlbar-history"));
	var entry = rdf.GetLiteral(urlToAdd);
    var index = entries.IndexOf(entry);

    if (index != -1) {
      // we've got it already. Remove it from its old place
      // and insert it to the top
      //dump("URL already in urlbar history\n");
      entries.RemoveElementAt(index, true);
    }

    // Otherwise, we've got a new URL in town. Add it!
    // Put the new entry at the front of the list.
    entries.InsertElementAt(entry, 1, true);

    // Remove any expired history items so that we don't let this grow
    // without bound.
    for (index = entries.GetCount(); index > MAX_HISTORY_ITEMS; --index) {
      entries.RemoveElementAt(index, true);
    }
  }
  }
 
function createMenuItem( aParent, aIndex, aValue)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "value", aValue );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function createCheckboxMenuItem( aParent, aIndex, aValue, aChecked)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "type", "checkbox" );
    menuitem.setAttribute( "value", aValue );
    menuitem.setAttribute( "index", aIndex );
    if (aChecked==true)
      menuitem.setAttribute( "checked", "true" );
    aParent.appendChild( menuitem );
  }

function deleteHistoryItems(aParent)
  {
    var children = aParent.childNodes;
    for (var i = 0; i < children.length; i++ )
      {
        var index = children[i].getAttribute( "index" );
        if (index) 		 
          aParent.removeChild( children[i] );
      }
  }

function updateGoMenu(event)
  {
    FillHistoryMenu(event.target, "go");
  }

