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
 *  - Kevin Puetz (puetz@iastate.edu)
 */

//
// Determine if d&d is on or not, off by default for beta but we want mozilla
// folks to be able to turn it on if they so desire.
//
var gDragDropEnabled = false;
var pref = null;
try {
  pref = Components.classes['component://netscape/preferences'];
  pref = pref.getService();
  pref = pref.QueryInterface(Components.interfaces.nsIPref);
}
catch (ex) {
  dump("failed to get prefs service!\n");
  pref = null;
}

try {
  gDragDropEnabled = pref.GetBoolPref("xpfe.dragdrop.enable");
}
catch (ex) {
  dump("assuming d&d is off for Personal Toolbar\n");
}  


function GeneralDrag ( event )
{
  if ( !gDragDropEnabled )
    return;

  dump("****** DRAG MADE IT TO TOPLEVEL WINDOW ********\n");
}

  
function BeginDragPersonalToolbar ( event )
{
  if ( !gDragDropEnabled )
    return;

  //XXX we rely on a capturer to already have determined which item the mouse was over
  //XXX and have set an attribute.
  
  // if the click is on the toolbar proper, ignore it. We only care about clicks on
  // items.
  var toolbar = document.getElementById("PersonalToolbar");
  if ( event.target == toolbar )
    return true;  // continue propagating the event
  
  // since the database is not on the toolbar, but on the innermost box, we need to 
  // make sure we can find it before we go any further. If we can't find it, we're
  // up the creek, but don't keep propagating the event.
  var childWithDatabase = document.getElementById("innermostBox");
  if ( ! childWithDatabase ) {
    event.preventBubble();
    return;
  }

  // pinkerton
  // right now, the issue of d&d and these popup menus is really wacky, so i'm punting
  // until I have time to fix it and we can come up with a good UI gesture (bug 19588). In
  // the meantime, if the target is a container, don't initiate the drag.
  var container = event.target.getAttribute("container");
  if ( container == "true" )
    return;
  
  var dragStarted = false;
  var dragService =
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    var trans = 
      Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
    if ( trans ) {
      trans.addDataFlavor("moz/toolbaritem");
      var genData = 
        Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
      trans.addDataFlavor("text/unicode");
      var genTextData = 
        Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);      
      if ( genData && genTextData ) {
      
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

	      trans.setTransferData ( "moz/toolbaritem", genData, id.length*2 );  // double byte data (len*2)
	      trans.setTransferData ( "text/unicode", genTextData, id.length*2 );  // double byte data
	      var transArray = 
	        Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
	      if ( transArray ) {
	        // put it into the transferable as an |nsISupports|
	        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
	        transArray.AppendElement(genTrans);
	        var nsIDragService = Components.interfaces.nsIDragService;
	        dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
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
  if ( !gDragDropEnabled )
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


//
// BeginDragContentArea
//
function BeginDragContentArea ( event )
{
  if ( !gDragDropEnabled )
    return;

  dump ("in content area; starting drag\n");
  var dragStarted = false;
  var dragService = Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService )
  {
    var trans = Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
    if ( trans )
    {
      var src = null;
      var htmlData = Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
      if ( htmlData )
      {
        trans.addDataFlavor("text/html");
        
        var htmlstring = null;
        switch (event.target.nodeName)
        {
          case 'AREA':
          case 'IMG':
            var imgsrc = event.target.getAttribute("src");
            var baseurl = window.content.location.href;
            src = baseurl + imgsrc;
            htmlstring = "<IMG src=\"" + src + "\">";
            dump("src is "+src+"\n");
            dump("htmlstring is "+htmlstring+"\n");
            break;
          
          case 'HR':
            break;
          
          case 'A':
            if ( event.target.href )
            {
              // link
              src = event.target.getAttribute("href");
              htmlstring = "<A href=\"" + src + "\">" + src + "</A>";
            }
            else if (event.target.name )
            {
              // named anchor
              src = event.target.getAttribute("name");
              htmlstring = "<A name=\"" + src + "\">" + src + "</A>"
            }
            break;
          
          default:
            dump("no handler found for " + event.target.nodeName + "\n");
          case '#text':
          case 'LI':
          case 'OL':
          case 'DD':
            src = enclosingLink(event.target);
            if ( src != "" )
              htmlstring = "<A href=\"" + src + "\">" + src + "</A>";
            else
              htmlstring = "<p>no html tag or link found; found "+event.target.nodeName;
            break;
        }
        
			  htmlData.data = htmlstring;
	      trans.setTransferData ( "text/html", htmlData, htmlstring.length*2 );  // double byte data (len*2)
      }
      
      var genTextData = 
        Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);      
      if ( genTextData && src != "")
      {
        trans.addDataFlavor("text/unicode");
			  genTextData.data = src; // should have been set if we had html
	      trans.setTransferData ( "text/unicode", genTextData, src.length*2 );  // double byte data
	    }
	    
      var transArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
      if ( transArray )
      {
        // put it into the transferable as an |nsISupports|
        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
        transArray.AppendElement(genTrans);
        var nsIDragService = Components.interfaces.nsIDragService;
        dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                            nsIDragService.DRAGDROP_ACTION_MOVE );
        dragStarted = true;
      }

    } // if transferable
  } // if drag service

  if ( dragStarted )               // don't propagate the event if a drag has begun
    event.preventBubble();
  
  return true;
  
}


//
// DragOverContentArea
//
// An example of how to handle drag-over. Looks for any of a handful of flavors and
// if it finds them it marks the dragSession that the drop is allowed.
//
function DragOverContentArea ( event )
{
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
      else if ( dragSession.isDataFlavorSupported("application/file") )
        validFlavor = true;
      //XXX other flavors here...
      
      if ( validFlavor ) {
        // XXX do some drag feedback here, set a style maybe???
        
        dragSession.canDrop = true;
        event.preventBubble();
      }
    }
  }
} // DragOverContentArea


//
// DropOnContentArea
//
// An example of how to handle a drop. Basically looks for the text flavor, extracts it,
// shoves it into the url bar, and loads the given URL. No checking is done to make sure
// this is a url ;)
//
function DropOnContentArea ( event )
{ 
  var dropAccepted = false;
  
  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    var dragSession = dragService.getCurrentSession();
    if ( dragSession ) {
      var trans = 
        Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
      if ( trans ) {
        trans.addDataFlavor("text/unicode");
        trans.addDataFlavor("application/file");
        for ( var i = 0; i < dragSession.numDropItems; ++i ) {
          var id = "";
          dragSession.getData ( trans, i );
          var dataObj = new Object();
          var bestFlavor = new Object();
          var len = new Object();
          trans.getAnyTransferData ( bestFlavor, dataObj, len );
          if ( bestFlavor.value == "text/unicode" ) {
            if ( dataObj ) dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
            if ( dataObj ) {
              // pull the URL out of the data object, two byte data
              var id = dataObj.data.substring(0, len.value / 2);
              dump("ID: '" + id + "'\n");
            }
          }
          else if ( bestFlavor.value == "application/file" ) {
            if ( dataObj ) dataObj = dataObj.value.QueryInterface(Components.interfaces.nsIFile);
            if ( dataObj ) {
              var fileURL = Components.classes["component://netscape/network/standard-url"]
                              .createInstance(Components.interfaces.nsIFileURL);
              if ( fileURL ) {
                fileURL.file = dataObj;
                id = fileURL.spec;
                dump("File dropped was: '" + id + "'\n");
              }
            }
          }
          
          // stuff it into the url field and go, baby, go!
          var urlBar = document.getElementById ( "urlbar" );
          urlBar.value = id;
          BrowserLoadURL();
            
          event.preventBubble();
        } // foreach drag item
      }
    }
  }
} // DropOnContentArea


//
// DragProxyIcon
//
// Called when the user is starting a drag from the proxy icon next to the URL bar. Basically
// just gets the url from the url bar and places the data (as plain text) in the drag service.
//
// This is by no means the final implementation, just another example of what you can do with
// JS. Much still left to do here.
// 
function DragProxyIcon ( event )
{
  var dragStarted = false;
  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    var trans = 
      Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
    if ( trans ) {
      var genTextData =
        Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
      if ( genTextData ) {
      
        // pull the url out of the url bar
        var urlBar = document.getElementById ( "urlbar" );
        if ( !urlBar )
          return;            
        var id = urlBar.value;
        genTextData.data = id;
      
        dump("ID: " + id + "\n");

        // add text/html flavor first
        var htmlData = Components.classes["component://netscape/supports-wstring"].createInstance();
        if ( htmlData )
          htmlData = htmlData.QueryInterface(Components.interfaces.nsISupportsWString);
        if ( htmlData ) {
          var htmlstring = "<A href=\"" + genTextData + "\">" + genTextData + "</A>";
          htmlData.data = htmlstring;
          trans.addDataFlavor("text/html");
          trans.setTransferData( "text/html", htmlData, htmlstring.length * 2);
        }
       
        // add the text/unicode flavor
        trans.addDataFlavor("text/unicode");
        trans.setTransferData ( "text/unicode", genTextData, id.length * 2 );  // double byte data
      
        var transArray = 
          Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
        if ( transArray ) {
          // put it into the transferable as an |nsISupports|
          var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
          transArray.AppendElement(genTrans);
          var nsIDragService = Components.interfaces.nsIDragService;
          dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                              nsIDragService.DRAGDROP_ACTION_MOVE );
          dragStarted = true;
        }
      } // if data object
    } // if transferable
  } // if drag service

  if ( dragStarted )               // don't propagate the event if a drag has begun
    event.preventBubble();
  
} // DragProxyIcon


//
// DragContentLink
//
// Called when the user is starting a drag from the a content-area link. Basically
// just gets the url from the link and places the data (as plain text) in the drag service.
//
// This is by no means the final implementation, just another example of what you can do with
// JS. Much still left to do here.
// 
function DragContentLink ( event )
{
  var target = event.target;
  var dragStarted = false;
  var dragService = 
    Components.classes["component://netscape/widget/dragservice"].getService(Components.interfaces.nsIDragService);
  if ( dragService ) {
    var trans = 
      Components.classes["component://netscape/widget/transferable"].createInstance(Components.interfaces.nsITransferable);
    if ( trans ) {
      trans.addDataFlavor("text/unicode");
      var genTextData =
        Components.classes["component://netscape/supports-wstring"].createInstance(Components.interfaces.nsISupportsWString);
      if ( genTextData ) {
      
        // pull the url out of the link
        var href = enclosingLink(target);
        if ( href == "" )
          return;
        var id = href
        genTextData.data = id;
      
        dump("ID: " + id + "\n");

        trans.setTransferData ( "text/unicode", genTextData, id.length * 2 );  // double byte data
        var transArray = 
          Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
        if ( transArray ) {
          // put it into the transferable as an |nsISupports|
          var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
          transArray.AppendElement(genTrans);
          var nsIDragService = Components.interfaces.nsIDragService;
          dragService.invokeDragSession ( transArray, null, nsIDragService.DRAGDROP_ACTION_COPY + 
                                              nsIDragService.DRAGDROP_ACTION_MOVE );
          dragStarted = true;
        }
      } // if data object
    } // if transferable
  } // if drag service

  if ( dragStarted )               // don't propagate the event if a drag has begun
    event.preventBubble();
  
} // DragContentLink
