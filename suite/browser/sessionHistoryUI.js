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
 * Jason Eager <jce2@po.cwru.edu>
 * (Some other netscape person who forgot to put the NPL header!)
 *
 */
const MAX_HISTORY_MENU_ITEMS = 15;
var rdf = Components.classes["component://netscape/rdf/rdf-service"].getService();
rdf = rdf.QueryInterface(Components.interfaces.nsIRDFService);
var localstore = rdf.GetDataSource("rdf:localstore");

function FillHistoryMenu( aParent, aMenu )
  {
    var shistory;
    // Get the content area docshell
    var docShell = null;
    var result = appCore.getContentDocShell(docShell);            
    if (docShell)
      {
        //Get the session history component from docshell
        docShell = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);
        if (docShell)
          {
            shistory = docShell.sessionHistory;
            if (shistory)
              {
                //Remove old entries if any
                deleteHistoryItems( aParent );
                var count = shistory.count;
                var index = shistory.index;
                switch (aMenu)
                  {
                    case "back":
                      var end = (index > MAX_HISTORY_MENU_ITEMS) ? index - MAX_HISTORY_MENU_ITEMS : 0;
                      for ( var j = index - 1; j >= end; j--) 
                        {
                          var entry = shistory.getEntryAtIndex(j, false);
                          if (entry) 
                            createMenuItem( aParent, j, entry.getTitle() );
                        }
                      break;
                    case "forward":
                      var end  = ((count-index) > MAX_HISTORY_MENU_ITEMS) ? index + MAX_HISTORY_MENU_ITEMS : count;
                      for ( var j = index + 1; j < end; j++)
                        {
                          var entry = shistory.getEntryAtIndex(j, false);
                          if (entry)
                            createMenuItem( aParent, j, entry.getTitle() );
                        }
                      break;
                    case "go":
                      var end = count > MAX_HISTORY_MENU_ITEMS 
                                  ? count - MAX_HISTORY_MENU_ITEMS 
                                  : 0;
                      for( var j = count - 1; j >= end; j-- )
                        {
                          var entry = shistory.getEntryAtIndex(j, false);
                          if (entry)
                            createMenuItem( aParent, j, entry.getTitle() );
                        }
                      break;
                  }
              }
          }
      }
  }
 
function executeUrlBarHistoryCommand( aTarget) 
  {
    var index = aTarget.getAttribute("index");
    var value = aTarget.getAttribute("value");
    if (index && value) 
      {
        gURLBar.value = value;
        BrowserLoadURL();
      }
  } 
 
function createUBHistoryMenu( aEvent )
  {
    var ubHistory = appCore.urlbarHistory;
    // dump("In createUbHistoryMenu\n");
	  if (localstore) {
	     var entries = localstore.GetTargets(rdf.GetResource("nc:urlbar-history"),
                                    rdf.GetResource("http://home.netscape.com/NC-rdf#child"),
                                    true);
       //dump("localstore found\n");
         var i= MAX_HISTORY_MENU_ITEMS;
		    // Delete any old menu items
		    deleteHistoryItems(aEvent);

        while (entries.hasMoreElements() && (i-- > 0)) {
		       var entry = entries.getNext();
			     if (entry) {
			        entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
              var url = entry.Value;
              //dump("CREATEUBHISTORYMENU menuitem = " + url + "\n");
			        createMenuItem(aEvent.target, i, url);

			     }
         }
      }
  }

function addToUrlbarHistory()
  {
    //var ubHistory = appCore.urlbarHistory;
	var urlToAdd = gURLBar.value;
	if (!urlToAdd)
	   return;
	if (localstore) {
	   var entries = localstore.GetTargets(rdf.GetResource("nc:urlbar-history"),
                                rdf.GetResource("http://home.netscape.com/NC-rdf#child"),
                                true);

       while (entries.hasMoreElements()) {
	     var entry = entries.getNext();
		 if (entry) {
		   entry = entry.QueryInterface(Components.interfaces.nsIRDFLiteral);
           var url = entry.Value;
		   if (url == urlToAdd) {
		      dump("URL already in urlbar history\n");
			  return;
		   }  
         }  //entry
       }  //while
	   dump("Adding " + urlToAdd + "to urlbar history\n");
	   localstore.Assert(rdf.GetResource("nc:urlbar-history"),
                    rdf.GetResource("http://home.netscape.com/NC-rdf#child"),
                    rdf.GetLiteral(urlToAdd),
                    true);
	} //localstore		
  }
  
function createMenuItem( aParent, aIndex, aValue)
  {
    var menuitem = document.createElement( "menuitem" );
    menuitem.setAttribute( "value", aValue );
    menuitem.setAttribute( "index", aIndex );
    aParent.appendChild( menuitem );
  }

function deleteHistoryItems( aEvent )
  {
    var children = aEvent.target.childNodes;
    for (var i = 0; i < children.length; i++ )
      {
        var index = children[i].getAttribute( "index" );
        if (index)
          aEvent.target.removeChild( children[i] );
      }
  }

function updateGoMenu(event)
  {
    appCore.updateGoMenu(event.target);
  }

