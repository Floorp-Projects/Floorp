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

function FillHistoryMenu(aParent, aMenu)
  {
    // Remove old entries if any
    deleteHistoryItems(aParent);

    var sessionHistory = getWebNavigation().sessionHistory;

    var count = sessionHistory.count;
    var index = sessionHistory.index;
    var end;
    var j;
    var entry;

    switch (aMenu)
      {
        case "back":
          end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
          for (j = index - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "forward":
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
          for (j = index + 1; j < end; j++)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "go":
          end = count > MAX_HISTORY_MENU_ITEMS ? count - MAX_HISTORY_MENU_ITEMS : 0;
          for (j = count - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createCheckboxMenuItem(aParent, j, entry.title, j==index);
            }
          break;
      }
  }

function executeUrlBarHistoryCommand( aTarget )
  {
    var index = aTarget.getAttribute("index");
    var value = aTarget.getAttribute("value");
    if (index != "nothing_available" && value)
      {
        loadShortcutOrURI(value);
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
       if (!entries)
          return;       
       var elements = entries.GetElements();
       if (!elements)
          return;         
       var index = 0;
       // create the nsIURI objects for comparing the 2 urls
       var uriToAdd = Components.classes["@mozilla.org/network/standard-url;1"]
                            .createInstance(Components.interfaces.nsIURI);
       uriToAdd.spec = urlToAdd;
       //dump("** URL entered = " + urlToAdd + "\n");
       var rdfUri = Components.classes["@mozilla.org/network/standard-url;1"]
                          .createInstance(Components.interfaces.nsIURI); 
       while(elements.hasMoreElements()) {
          entry = elements.getNext();
          if (entry) {
             index ++;
             entry= entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
             var rdfValue = entry.Value;             
             //dump("**** value obtained from RDF " + rdfValue + "\n");
             rdfUri.spec = rdfValue;
             if (rdfUri.equals(uriToAdd)) {
                 // URI already present in the database
                 // Remove it from its current position.
                 // It is inserted to the top after the while loop.
                 //dump("*** URL are the same \n");
                 entries.RemoveElementAt(index, true);
                 break;                 
             }             
          }    
       }   // while     

       var entry = rdf.GetLiteral(urlToAdd);
       // Otherwise, we've got a new URL in town. Add it!
       // Put the value as it was typed by the user in to RDF
       // Insert it to the beginning of the list.
       entries.InsertElementAt(entry, 1, true);

       // Remove any expired history items so that we don't let 
       // this grow without bound.
       for (index = entries.GetCount(); index > MAX_HISTORY_ITEMS; --index) {
           entries.RemoveElementAt(index, true);
        }  // for
   }  // localstore
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

