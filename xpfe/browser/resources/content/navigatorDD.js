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
 *  - Kevin Puetz (puetzk@iastate.edu)
 *  - Ben Goodger <ben@netscape.com>
 */
/*
var personalToolbarObserver = {
  onDragStart: function (aEvent)
    {
      var personalToolbar = document.getElementById("PersonalToolbar");
      if (aEvent.target == personalToolbar)
        return true;
        
      var childWithDatabase = document.getElementById("innermostBox");
      if (!childWithDatabase)
        {
          event.preventBubble();
          return;
        }

      var uri = aEvent.target.id;
      var flavourList = { };
      flavourList["moz/toolbaritem"] = { width: 2, data: uri };
      flavourList["text/unicode"] = { width: 2, data: uri };
    },
  
}; 

			  var id = event.target.getAttribute("id");
			  genData.data = id;
			  genTextData.data = id;
			      
			  dump("ID: " + id + "\n");

			  var database = childWithDatabase.database;
			  var rdf = 
			    Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
			  if ((!rdf) || (!database))  return(false);

			  // make sure its a bookmark, bookmark separator, or bookmark folder
			  var src = rdf.GetResource(id, true);
			  var prop = rdf.GetResource("http://www.w3.org/1999/02/22-rdf-syntax-ns#type", true);
			  var target = database.GetTarget(src, prop, true);
*/
/*
pinkerton
this doesn't work anymore (target is null), not sure why.
  if (target) target = target.QueryInterface(Components.interfaces.nsIRDFResource);
  if (target) target = target.Value;
  if ((!target) || (target == "")) {dump("BAD\n"); return(false);}

  dump("Type: '" + target + "'\n");

  if ((target != "http://home.netscape.com/NC-rdf#BookmarkSeparator") &&
     (target != "http://home.netscape.com/NC-rdf#Bookmark") &&
     (target != "http://home.netscape.com/NC-rdf#Folder"))  return(false);
*/
/*
	      trans.setTransferData ( "moz/toolbaritem", genData, id.length*2 );  // double byte data (len*2)
	      trans.setTransferData ( "text/unicode", genTextData, id.length*2 );  // double byte data
	      var transArray = 
	        Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
	      if ( transArray ) {
	        // put it into the transferable as an |nsISupports|
	        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
	        transArray.AppendElement(genTrans);
	        var nsIDragService = Components.interfaces.nsIDragService;
	        dragService.invokeDragSession ( event.target, transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
	                                            nsIDragService.DRAGDROP_ACTION_MOVE );
	        dragStarted = true;
	      }
      } // if data object
    } // if transferable
  } // if drag service

  if ( dragStarted )               // don't propagate the event if a drag has begun
    event.preventBubble();
  
  return true;
  
} // BeginDragPersonalToolbar
*/

function DropPersonalToolbar ( event )
{ 
  if ( !gDragDropEnabled )
    return;
  
  var dropAccepted = false;
  
  var dragService =
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    var dragSession = dragService.getCurrentSession();
    if ( dragSession ) {
      var trans =
        Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
      if ( trans ) {

        // get references to various services/resources once up-front
        var personalToolbarRes = null;
        
        var rdf = 
          Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);
        if (rdf)
          personalToolbarRes = rdf.GetResource("NC:PersonalToolbarFolder");

        var rdfc = 
          Components.classes["component://netscape/rdf/container"].getService(Components.interfaces.nsIRDFContainer);
        if ( !rdfc ) return false;
        
        trans.addDataFlavor("moz/toolbaritem");
        for ( var i = 0; i < dragSession.numDropItems; ++i ) {
          dragSession.getData ( trans, i );
          var dataObj = new Object();
          var bestFlavor = new Object();
          var len = new Object();
          trans.getAnyTransferData ( bestFlavor, dataObj, len );
          if ( dataObj ) dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
          if ( dataObj ) {
          
            // remember len is in bytes, not chars
            var id = dataObj.data.substring(0, len.value / 2);
            dump("ID: '" + id + "'\n");
            
            var objectRes = rdf.GetResource(id, true);

            dragSession.canDrop = true;
            dropAccepted = true;
                
            var boxWithDatabase = document.getElementById("innermostBox");
            var database = boxWithDatabase.database;
            if (database && rdf && rdfc && personalToolbarRes && objectRes)
            {
              
              rdfc.Init(database, personalToolbarRes);

              // Note: RDF Sequences are one-based, not zero-based

              // XXX figure out the REAL index to insert at;
              // for the moment, insert it as the first element (pos=1)
              var newIndex = 1;

              var currentIndex = rdfc.IndexOf(objectRes);
              if (currentIndex > 0)
              {
                dump("Element '" + id + "' was at position # " + currentIndex + "\n");
                rdfc.RemoveElement(objectRes, true);
                dump("Element '" + id + "' removed from position # " + currentIndex + "\n");
              }
              rdfc.InsertElementAt(objectRes, newIndex, true);
              dump("Element '" + id + "' re-inserted at new position # " + newIndex + ".\n");
            }

          } 
            
        } // foreach drag item
    
      } // if transferable
    } // if dragsession
  } // if dragservice
  
  if ( dropAccepted )               // don't propagate the event if we did the drop
    event.preventBubble();

} // DropPersonalToolbar


function DragOverPersonalToolbar ( event )
{
  if ( 1 )
    return;
  
  var validFlavor = false;
  var dragSession = null;

  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    dragSession = dragService.getCurrentSession();
    if ( dragSession ) {
      if ( dragSession.isDataFlavorSupported("moz/toolbaritem") )
        validFlavor = true;
      else if ( dragSession.isDataFlavorSupported("text/unicode") )
        validFlavor = true;
      //XXX other flavors here...

      // touch the attribute to trigger the repaint with the drop feedback.
      if ( validFlavor ) {
        //XXX this is really slow and likes to refresh N times per second.
        var toolbar = document.getElementById("PersonalToolbar");
        toolbar.setAttribute ( "dd-triggerrepaint", 0 );
        dragSession.canDrop = true;
        event.preventBubble();
      }
    }
  }

} // DragOverPersonalToolbar


var contentAreaDNDObserver = {
  onDragStart: function (aEvent)
    {  
      var htmlstring = null;
      var textstring = null;
      var domselection = window._content.getSelection();
      if (domselection && !domselection.isCollapsed && 
          domselection.containsNode(aEvent.target,false))
        {
          // the window has a selection so we should grab that rather than looking for specific elements
          htmlstring = domselection.toString("text/html", 128+256, 0);
          textstring = domselection.toString("text/plain", 0, 0);
        }
      else 
        {
          switch (aEvent.target.localName) 
            {
              case 'AREA':
              case 'IMG':
                var imgsrc = aEvent.target.getAttribute("src");
                var baseurl = window._content.location.href;
                // need to do some stuff with the window._content.location here (path?) 
                // to get base URL for image.
                textstring = imgsrc;
                htmlstring = "<img src=\"" + textstring + "\">";
                break;
              case 'A':
                if (aEvent.target.href)
                  {
                    textstring = aEvent.target.getAttribute("href");
                    htmlstring = "<a href=\"" + textstring + "\">" + textstring + "</a>";
                  }
                else if (aEvent.target.name)
                  {
                    textstring = aEvent.target.getAttribute("name");
                    htmlstring = "<a name=\"" + textstring + "\">" + textstring + "</a>"
                  }
                break;
              default:
              case '#text':
              case 'LI':
              case 'OL':
              case 'DD':
                textstring = enclosingLink(aEvent.target);
                if (textstring != "")
                  htmlstring = "<a href=\"" + textstring + "\">" + textstring + "</a>";
                else
                  throw Components.results.NS_ERROR_FAILURE;
                break;
            }
        }
  
      var flavourList = { };
      flavourList["text/html"] = { width: 2, data: htmlstring };
      flavourList["text/unicode"] = { width: 2, data: textstring };
      return flavourList;
    },

  onDrop: function (aEvent, aData)
    {
      var aData = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(aData);
      if (url.length == 0)
        return;
      // valid urls don't contain spaces ' '; if we have a space it isn't a valid url so bail out
      var urlstr = url.toString();
      if ( urlstr.indexOf(" ", 0) != -1 )
        return;

      var urlBar = document.getElementById("urlbar");
      urlBar.value = url;
      BrowserLoadURL();
    },

  getSupportedFlavours: function ()
    {
      var flavourList = { };
      //flavourList["moz/toolbaritem"] = { width: 2 };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["application/x-moz-file"] = { width: 2, iid: "nsIFile" };
      return flavourList;
    },
};

//
// DragProxyIcon
//
// Called when the user is starting a drag from the proxy icon next to the URL bar. Basically
// just gets the url from the url bar and places the data (as plain text) in the drag service.
//
// This is by no means the final implementation, just another example of what you can do with
// JS. Much still left to do here.
// 

var proxyIconDNDObserver = {
  onDragStart: function ()
    {
      var urlBar = document.getElementById("urlbar");
      var flavourList = { };
      flavourList["text/unicode"] = { width: 2, data: urlBar.value };
//      flavourList["text/x-moz-url"] = { width: 2, data: urlBar.value + " " + window.title };
//*** hack until bug 41984 is fixed. uncomment the above line, it is the "correct" fix
      flavourList["text/x-moz-url"] = { width: 2, data: urlBar.value + " " + "( TEMP TITLE )" };
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";
      flavourList["text/html"] = { width: 2, data: htmlString };
      return flavourList;
    }
};

var homeButtonObserver = {
  onDrop: function (aEvent, aData)
    {
      var aData = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(aData);
      var showDialog = nsPreferences.getBoolPref("browser.homepage.enable_home_button_drop", false);
      var setHomepage;
      if (showDialog)
        {
          var commonDialogService = nsJSComponentManager.getService("component://netscape/appshell/commonDialogs",
                                                                    "nsICommonDialogs");
          var block = nsJSComponentManager.createInstanceByID("c01ad085-4915-11d3-b7a0-85cf-55c3523c",
                                                              "nsIDialogParamBlock");
          var checkValue = { value: true };
          var pressedVal = { };                            
          var promptTitle = bundle.GetStringFromName("droponhometitle");
          var promptMsg   = bundle.GetStringFromName("droponhomemsg");
          var checkMsg    = bundle.GetStringFromName("dontremindme");
          var okButton    = bundle.GetStringFromName("droponhomeokbutton");
          var iconURL     = "chrome://navigator/skin/home.gif"; // evil evil common dialog code! evil! 
/*
          block.SetInt(2, 2);
          block.SetString(0, bundle.GetStringFromName("droponhomemsg"));
          block.SetString(3, bundle.GetStringFromName("droponhometitle"));
          block.SetString(2, "chrome://navigator/skin/home.gif");
          block.SetString(1, bundle.GetStringFromName("dontremindme"));
          block.SetInt(1, 1); // checkbox is checked
          block.SetString(8, bundle.GetStringFromName("droponhomeokbutton"));
          
*/
          commonDialogService.UniversalDialog(window, null, promptTitle, promptMsg, checkMsg, 
                                              okButton, null, null, null, null, null, { }, { },
                                              iconURL, checkValue, 2, 0, null, pressedVal);
          nsPreferences.setBoolPref("browser.homepage.enable_home_button_drop", checkValue.value);

          setHomepage = pressedVal.value == 0 ? true : false;
        }
      else
        setHomepage = true;
      if (setHomepage) 
        {
          nsPreferences.setUnicharPref("browser.startup.homepage", url);                                           
          setTooltipText("homebutton", url);
        }
    },
    
  onDragOver: function (aEvent, aFlavour)
    {
      var homeButton = aEvent.target;
      // preliminary attribute name for now
      homeButton.setAttribute("home-dragover","true");
		  var statusTextFld = document.getElementById("statusbar-display");
      gStatus = gStatus ? gStatus : statusTextFld.value;
      statusTextFld.setAttribute("value", bundle.GetStringFromName("droponhomebutton"));
    },
    
  onDragExit: function ()
    {
      var homeButton = document.getElementById("homebutton");
      homeButton.removeAttribute("home-dragover");
      statusTextFld.setAttribute("value", gStatus);
      gStatus = null;
    },
        
  onDragStart: function ()
    {
      var homepage = nsPreferences.getLocalizedUnicharPref("browser.startup.homepage", "about:blank");
      var flavourList = { };
      flavourList["text/unicode"] = { width: 2, data: homepage };
      flavourList["text/x-moz-url"] = { width: 2, data: homepage };
      var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
      flavourList["text/html"] = { width: 2, data: htmlString };
      return flavourList;
    },
  
  getSupportedFlavours: function ()
    {
      var flavourList = { };
      //flavourList["moz/toolbaritem"] = { width: 2 };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["application/x-moz-file"] = { width: 2, iid: "nsIFile" };
      return flavourList;
    },  
};

function retrieveURLFromData (aData)
  {
    switch (aData.flavour)
      {
        case "text/unicode":
          return aData.data.data; // XXX this is busted. 
          break;
        case "application/x-moz-file":
          var dataObj = aData.data.data.QueryInterface(Components.interfaces.nsIFile);
          if (dataObj)
            {
              var fileURL = nsJSComponentManager.createInstance("component://netscape/network/standard-url",
                                                                "nsIFileURL");
              fileURL.file = dataObj;
              return fileURL.spec;
            }
      }             
    return null;                                                         
  }
