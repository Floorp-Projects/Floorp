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

var gRDFService = nsJSComponentManager.getService("@mozilla.org/rdf/rdf-service;1",
                                                  "nsIRDFService"); 

function RDF(aType) 
  {
    return "http://www.w3.org/1999/02/22-rdf-syntax-ns#" + aType;
  }
function NC_RDF(aType)
  {
    return "http://home.netscape.com/NC-rdf#" + aType;  
  }

var RDFUtils = {
  getResource: function(aString)
    {
      return gRDFService.GetResource(aString, true);
    },
  
  getTarget: function(aDS, aSourceID, aPropertyID)
    {
      var source = this.getResource(aSourceID);
      var property = this.getResource(aPropertyID);
      return aDS.GetTarget(source, property, true);
    },
  
  getValueFromResource: function(aResource)
    {
      aResource = aResource.QueryInterface(Components.interfaces.nsIRDFResource);
      return aResource ? target.Value : null;
    }
};

function isBookmark(aURI)
  {
    var rv = true;
    var typeValue = RDFUtils.getTarget(childWithDatabase.database, uri, RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    if (typeValue != NC_RDF("BookmarkSeparator") && 
        typeValue != NC_RDF("Bookmark") &&
        typeValue != NC_RDF("Folder")) 
      rv = false;
    return rv;
  }

function isPToolbarDNDEnabled()
  {
    var prefs = nsJSComponentManager.getService("@mozilla.org/preferences;1",
                                                "nsIPref");
    var dragAndDropEnabled = false;                                                  
    try {
      dragAndDropEnabled = prefs.GetBoolPref("browser.enable.tb_dnd");
    }
    catch(e) {
    }
    
    return dragAndDropEnabled;
  }
  
var personalToolbarObserver = {
  onDragStart: function (aEvent)
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var personalToolbar = document.getElementById("PersonalToolbar");
      if (aEvent.target == personalToolbar)
        return null;
        
      var childWithDatabase = document.getElementById("innermostBox");
      var uri = aEvent.target.id;
      //if (!isBookmark(uri)) 
      //  return;

      var title = aEvent.target.value;
      var htmlString = "<A HREF='" + uri + "'>" + title + "</A>";

      var flavourList = { };
      flavourList["moz/toolbaritem"] = { width: 2, data: uri };
      flavourList["text/x-moz-url"] = { width: 2, data: escape(uri) + " " + "[ TEMP TITLE ]" };
      flavourList["text/html"] = { width: 2, data: htmlString };
      flavourList["text/unicode"] = { width: 2, data: uri };
      return flavourList;
    },
  
  onDrop: function (aEvent, aData, aDragSession) 
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var element = aData.data.data;
      var dropElement = aEvent.target.id;
      var elementRes = RDFUtils.getResource(element);
      var dropElementRes = RDFUtils.getResource(dropElement);
      var personalToolbarRes = RDFUtils.getResource("NC:PersonalToolbarFolder");
      
      var childDB = document.getElementById("innermostBox").database;
      var rdfContainer = nsJSComponentManager.createInstance("@mozilla.org/rdf/container;1",
                                                             "nsIRDFContainer");
      rdfContainer.Init(childDB, personalToolbarRes);
      
      var elementIsOnToolbar = rdfContainer.IndexOf(elementRes);
      if (elementIsOnToolbar > 0) 
        rdfContainer.RemoveElement(elementRes, true);
      else if (dropIndex == -1)
        {
          // look up this URL's title in global history
          var potentialTitle = null;
          var historyDS = gRDFService.GetDataSource("rdf:history");
          var historyEntry = gRDFService.GetResource(element);
          var historyTitleProperty = gRDFService.GetResource(NC_RDF("Name"));
          var titleFromHistory = historyDS.GetTarget(historyEntry, historyTitleProperty, true);
          if (titleFromHistory)
            titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (titleFromHistory)
            potentialTitle = titleFromHistory.Value;
          linkTitle = potentialTitle ? potentialTitle : element;
          childDB.Assert(gRDFService.GetResource(element, true), 
                         gRDFService.GetResource(NC_RDF("Name"), true),
                         gRDFService.GetLiteral(linkTitle),
                         true);
        }
      var dropIndex = rdfContainer.IndexOf(dropElementRes);
      // determine the drop position
      var dropPosition = this.determineDropPosition(aEvent);
      switch (dropPosition) {
      case -1:
        rdfContainer.InsertElementAt(elementRes, dropIndex, true);
        break;
      case 0:
        // do something here to drop into subfolders
        var childContainer = nsJSComponentManager.createInstance("@mozilla.org/rdf/container;1",
                                                                 "nsIRDFContainer");
        childContainer.Init(childDB, dropElementRes);
        childContainer.AppendElement(elementRes);
        break;
      case 1:
      default:
        rdfContainer.InsertElementAt(elementRes, dropIndex+1, true);
        break;
      }
      return true;
    },
  
  mCurrentDragOverButton: null,
  mCurrentDragPosition: null,
  
  onDragExit: function (aEvent, aDragSession)
    {
      if (this.mCurrentDragOverButton)
        {
          this.mCurrentDragOverButton.removeAttribute("dragover-left");
          this.mCurrentDragOverButton.removeAttribute("dragover-right");
          this.mCurrentDragOverButton.removeAttribute("dragover-top");
          this.mCurrentDragOverButton.removeAttribute("open");
        }
    },
  
  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;

      var dropPosition = this.determineDropPosition(aEvent);
      
      if (this.mCurrentDragOverButton != aEvent.target ||
          (this.mCurrentDragOverButton == aEvent.target && 
           this.mCurrentDragPosition != dropPosition)) 
        {
          if (this.mCurrentDragOverButton)
            {
              this.mCurrentDragOverButton.removeAttribute("dragover-left");
              this.mCurrentDragOverButton.removeAttribute("dragover-right");
              this.mCurrentDragOverButton.removeAttribute("dragover-top");
              this.mCurrentDragOverButton.removeAttribute("open");
            }
          this.mCurrentDragOverButton = aEvent.target;
          this.mCurrentDragPosition = dropPosition;
        }
      
      switch (dropPosition) 
        {
          case -1: 
            aEvent.target.setAttribute("dragover-left", "true");
            break;
          case 1:
            aEvent.target.setAttribute("dragover-right", "true");
            break;
          case 0:
          default:
            if (aEvent.target.getAttribute("container") == "true") {
              aEvent.target.setAttribute("dragover-top", "true");
              //cant open a menu during a drag! suck!
              //aEvent.target.setAttribute("open", "true");
            }
            break;
        }
        
       return true;
    },

  getSupportedFlavours: function ()
    {
      // temporary
      if (!isPToolbarDNDEnabled())
        return false;
        
      var flavourList = { };
      flavourList["moz/toolbaritem"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      return flavourList;
    },

  determineDropPosition: function (aEvent)
    {
      var overButton = aEvent.target;
      var overButtonBoxObject = overButton.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);

      if (aEvent.clientX < (overButtonBoxObject.x + overButtonBoxObject.width/3)) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height)
            return -1;
        }
      else if (aEvent.clientX > (overButtonBoxObject.x + 2*(overButtonBoxObject.width/3))) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height) 
            return 1;
        }
      else if (aEvent.clientX > (overButtonBoxObject.x + overButtonBoxObject.width/3) &&
               aEvent.clientX < ((overButtonBoxObject.x + 2*(overButtonBoxObject.width/3)))) 
        {
          if (aEvent.clientY > overButtonBoxObject.y && 
              aEvent.clientY < overButtonBoxObject.y + overButtonBoxObject.height) 
            return 0;
        }
        
      return 0;
    }
}; 

var contentAreaDNDObserver = {
  onDragStart: function (aEvent)
    {  
      dump("dragstart\n");
      var htmlstring = null;
      var textstring = null;
      var isLink = false;
      var domselection = window._content.getSelection();
      if (domselection && !domselection.isCollapsed && 
          domselection.containsNode(aEvent.target,false))
        {
          var privateSelection = domselection.QueryInterface(Components.interfaces.nsISelectionPrivate);
          if (privateSelection)
          {
            // the window has a selection so we should grab that rather than looking for specific elements
            htmlstring = privateSelection.toStringWithFormat("text/html", 128+256, 0);
            textstring = privateSelection.toStringWithFormat("text/plain", 0, 0);
            dump("we cool?\n");
          }
        }
      else 
        {
          dump("dragging DOM node: <" + aEvent.target.localName + ">\n");
          switch (aEvent.target.localName) 
            {
              case 'AREA':
              case 'IMG':
                var imgsrc = aEvent.target.getAttribute("src");
                var baseurl = window._content.location.href;
                // need to do some stuff with the window._content.location (path?) 
                // to get base URL for image.

                textstring = imgsrc;
                htmlstring = "<img src=\"" + textstring + "\">";
                if ((linkNode = this.findEnclosingLink(aEvent.target))) {
                  htmlstring = '<a href="' + linkNode.href + '">' + htmlstring + '</a>';
                  textstring = linkNode.href;
                }
                break;
              case 'A':
                isLink = true;
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
                var node = enclosingLink(aEvent.target);
                textstring = "";
                //select node now!
                if (node)
                  textstring = node.href;
                if (textstring != "")
                {
                  htmlstring = "<a href=\"" + textstring + "\">" + textstring + "</a>";
                  var parent = node.parentNode;
                  if (parent)
                  {
                    var nodelist = parent.childNodes;
                    var index;
                    for (index = 0; index<nodelist.length; index++)
                    {
                      if (nodelist.item(index) == node)
                        break;
                    }
                    if (index >= nodelist.length)
                        throw Components.results.NS_ERROR_FAILURE;
                    if (domselection)
                    {
                      domselection.collapse(parent,index);
                      domselection.extend(parent,index+1);
                    }
                  }
                }
                else
                  throw Components.results.NS_ERROR_FAILURE;
                break;
            }
        }

      var flavourList = { };
      flavourList["text/html"] = { width: 2, data: htmlstring };
      if (isLink)
        flavourList["text/x-moz-url"] = { width: 2, data: escape(textstring) + " " + "( TEMP TITLE )" };
      flavourList["text/unicode"] = { width: 2, data: textstring };
      return flavourList;
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      // if the drag originated w/in this content area, bail early. This avoids loading
      // a URL dragged from the content area into the very same content area (which is
      // almost never the desired action). This code is a bit too simplistic and may
      // have problems with nested frames, but that's not my problem ;)
      if ( aDragSession.sourceDocument == window._content.document )
        aDragSession.canDrop = false;
    },

  onDrop: function (aEvent, aData, aDragSession)
    {
      var data = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(data);
      if (url.length == 0)
        return;
      // this is a hacky fix for #40911 so that dragging
      // and dropping text from the same window into itself
      // does not load that text as a URL.
      // see bug #52519 for related problems with this fix
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
      flavourList["text/x-moz-url"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["application/x-moz-file"] = { width: 2, iid: "nsIFile" };
      return flavourList;
    },

  findEnclosingLink: function (aNode)  
    {  
      while (aNode) {
        var nodeName = aNode.localName;
        if (!nodeName) return null;
        nodeName = nodeName.toLowerCase();
        if (!nodeName || nodeName == "body" || 
            nodeName == "html" || nodeName == "#document")
          return null;
        if (nodeName == "a" && aNode.href)
          return aNode;
        aNode = aNode.parentNode;
      }
      return null;
    }
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
      flavourList["text/x-moz-url"] = { width: 2, data: escape(urlBar.value) + " " + window.title };
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";
      flavourList["text/html"] = { width: 2, data: htmlString };
      return flavourList;
    }
};

var homeButtonObserver = {
  onDrop: function (aEvent, aData, aDragSession)
    {
      var data = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(data);
      var showDialog = nsPreferences.getBoolPref("browser.homepage.enable_home_button_drop", false);
      var setHomepage;
      if (showDialog)
        {
          var commonDialogService = nsJSComponentManager.getService("@mozilla.org/appshell/commonDialogs;1",
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
    
  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var homeButton = aEvent.target;
      // preliminary attribute name for now
      homeButton.setAttribute("home-dragover","true");
		  var statusTextFld = document.getElementById("statusbar-display");
      gStatus = gStatus ? gStatus : statusTextFld.value;
      statusTextFld.setAttribute("value", bundle.GetStringFromName("droponhomebutton"));
    },
    
  onDragExit: function (aEvent, aDragSession)
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
      flavourList["text/x-moz-url"] = { width: 2, data: escape(homepage) + " " + "[ TEMP - Home Page ]" };
      flavourList["text/html"] = { width: 2, data: htmlString };
      flavourList["text/unicode"] = { width: 2, data: homepage };
      var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
      return flavourList;
    },
  
  getSupportedFlavours: function ()
    {
      var flavourList = { };
      //flavourList["moz/toolbaritem"] = { width: 2 };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      flavourList["application/x-moz-file"] = { width: 2, iid: "nsIFile" };
      return flavourList;
    }
};

function retrieveURLFromData (aData)
  {
    switch (aData.flavour)
      {
        case "text/unicode":
          return aData.data.data; // XXX this is busted. 
        case "text/x-moz-url":
          return unescape(aData.data.data); // XXX this is busted. 
          break;
        case "application/x-moz-file":
          var dataObj = aData.data.data.QueryInterface(Components.interfaces.nsIFile);
          if (dataObj)
            {
              var fileURL = nsJSComponentManager.createInstance("@mozilla.org/network/standard-url;1",
                                                                "nsIFileURL");
              fileURL.file = dataObj;
              return fileURL.spec;
            }
      }             
    return null;                                                         
  }
