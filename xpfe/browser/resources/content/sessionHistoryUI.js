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
 *  Jason Eager <jce2@po.cwru.edu>
 *  Blake Ross <BlakeR1234@aol.com>
 *  Peter Annema <disttsc@bart.nl>
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
          if ((index - 1) < end) return false;
          for (j = index - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "forward":
          end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
          if ((index + 1) >= end) return false;
          for (j = index + 1; j < end; j++)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createMenuItem(aParent, j, entry.title);
            }
          break;
        case "go":
        if (count > 0) aParent.lastChild.removeAttribute( "hidden" );
          end = count > MAX_HISTORY_MENU_ITEMS ? count - MAX_HISTORY_MENU_ITEMS : 0;
          for (j = count - 1; j >= end; j--)
            {
              entry = sessionHistory.getEntryAtIndex(j, false);
              if (entry)
                createCheckboxMenuItem(aParent, j, entry.title, j==index);
            }
          break;
      }
    return true;
  }

function executeUrlBarHistoryCommand( aTarget )
  {
    var index = aTarget.getAttribute("index");
    var label = aTarget.getAttribute("label");
    if (index != "nothing_available" && label)
      {
        var uri = getShortcutOrURI(label);
        if (gURLBar) {
          gURLBar.value = uri;
          BrowserLoadURL();
        }
        else
          loadURI(uri);
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
        //Create the "Nothing Available" Menu item and disable it.
        var na = gNavigatorBundle.getString("nothingAvailable");
        createMenuItem(aParent, "nothing_available", na);
        aParent.firstChild.setAttribute("disabled", "true");
      }

      while (entries.hasMoreElements() && (i-- > 0)) {
        var entry = entries.getNext();
        if (entry) {
          try {
            entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
          } catch(ex) {
            // XXXbar not an nsIRDFLiteral for some reason. see 90337.
            continue;
          }
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
  if (urlToAdd.search(/[\x00-\x1F]/) != -1) // don't store bad URLs
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
       var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                     .getService(Components.interfaces.nsIIOService);
       
       try {
         ioService.extractScheme(urlToAdd, {}, {});
       } catch(e) {
         urlToAdd = "http://" + urlToAdd;
       }
       
       try {
         var uriToAdd  = ioService.newURI(urlToAdd, null);
       }
       catch(e) {
         // it isn't a valid url
         // we'll leave uriToAdd as "undefined" and handle that later
       }

       while(elements.hasMoreElements()) {
          entry = elements.getNext();
          if (!entry) continue;

          index ++;
          try {
            entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
          } catch(ex) {
            // XXXbar not an nsIRDFLiteral for some reason. see 90337.
            continue;
          }
          var rdfValue = entry.Value;

          try {
            ioService.extractScheme(rdfValue, {}, {});
          } catch(e) {
            rdfValue = "http://" + rdfValue;
          }

          if (uriToAdd) {
            try {
              var rdfUri = ioService.newURI(rdfValue, null);
                 
              if (rdfUri.equals(uriToAdd)) {
                // URI already present in the database
                // Remove it from its current position.
                // It is inserted to the top after the while loop.
                entries.RemoveElementAt(index, true);
                break;
              }
            }
               
            // the uri is still not recognized by the ioservice
            catch(ex) {
              // no problem, we'll handle this below
            }
          }

          // if we got this far, then something is funky with the URIs,
          // so we need to do a straight string compare of the raw strings
          if (urlToAdd == rdfValue) {
            entries.RemoveElementAt(index, true);
            break;
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

function createMenuItem( aParent, aIndex, aLabel)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "label", aLabel );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function createCheckboxMenuItem( aParent, aIndex, aLabel, aChecked)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "type", "checkbox" );
    menuitem.setAttribute( "label", aLabel );
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

