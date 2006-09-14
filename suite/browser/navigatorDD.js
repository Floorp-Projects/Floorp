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

var gRDFService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                            .getService(Components.interfaces.nsIRDFService);

function _RDF(aType)
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
      return aResource ? aResource.Value : null;
    }
};

function isBookmark(aURI)
  {
    var db = document.getElementById("innermostBox").database;
    var typeValue = RDFUtils.getTarget(db, aURI, _RDF("type"));
    typeValue = RDFUtils.getValueFromResource(typeValue);
    return (typeValue == NC_RDF("BookmarkSeparator") ||
            typeValue == NC_RDF("Bookmark") ||
            typeValue == NC_RDF("Folder"))
  }

var personalToolbarObserver = {
  DROP_BEFORE: -1,
  DROP_ON: 0,
  DROP_AFTER: 1,
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      // Prevent dragging out of menus on non Win32 platforms. 
      // a) on Mac drag from menus is generally regarded as being satanic
      // b) on Linux, this causes an X-server crash, see bug 79003. 
      // Since we're not doing D&D into menus properly at this point, it seems
      // fair enough to disable it on non-Win32 platforms. There is no hang
      // or crash associated with this on Windows, so we'll leave the functionality
      // there. 
      if (navigator.platform != "Win32" && aEvent.target.localName != "button")
        return;


      if (aEvent.target.localName == "menu" || aEvent.target.localName == "menubutton") {                                 
        if (aEvent.target.getAttribute("type") == "http://home.netscape.com/NC-rdf#Folder") {
          var child = aEvent.target.childNodes[0];                               
          if (child && child.localName == "menupopup")                                     
            child.closePopup();                                                  
          else {                                                                 
            var parent = aEvent.target.parentNode;                               
            if (parent && parent.localName == "menupopup")                                 
            parent.closePopup();                                               
          }                                                                      
        }
      }                                                                         

      var personalToolbar = document.getElementById("PersonalToolbar");
      if (aEvent.target == personalToolbar) return;

      var db = document.getElementById("innermostBox").database;
      var uri = aEvent.target.id;
      if (!isBookmark(uri)) return;
      var url = RDFUtils.getTarget(db, uri, NC_RDF("URL"));
      if (url)
        url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        url = "";
      var name = RDFUtils.getTarget(db, uri, NC_RDF("Name"));
      if (name)
        name = name.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
      else
        name = "";
      var urlString = url + "\n" + name;
      var htmlString = "<A HREF='" + uri + "'>" + name + "</A>";
      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("moz/rdfitem", uri);
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/html", htmlString);
      aXferData.data.addDataForFlavour("text/unicode", url);
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var xferData = aXferData.data.split("\n");
      var elementRes = RDFUtils.getResource(xferData[0]);
      var personalToolbarRes = RDFUtils.getResource("NC:PersonalToolbarFolder");

      var inner = document.getElementById("innermostBox");
      var childDB = inner.database;
      const kCtrContractID = "@mozilla.org/rdf/container;1";
      const kCtrIID = Components.interfaces.nsIRDFContainer;
      var rdfContainer = Components.classes[kCtrContractID].createInstance(kCtrIID);

      // if dragged url is already bookmarked, remove it from current location first
      var parentContainer = this.findParentContainer(aDragSession.sourceNode);
      if (parentContainer)
        {
          rdfContainer.Init(childDB, parentContainer);
          rdfContainer.RemoveElement(elementRes, false);
        }

      // determine charset of link
      var linkCharset = aDragSession.sourceDocument ? aDragSession.sourceDocument.characterSet : null;
      // determine title of link
      var linkTitle;
      
      // look it up in bookmarks
      var bookmarksDS = gRDFService.GetDataSource("rdf:bookmarks");
      var nameRes = RDFUtils.getResource(NC_RDF("Name"));
      var nameFromBookmarks = bookmarksDS.GetTarget(elementRes, nameRes, true);
      if (nameFromBookmarks)
        nameFromBookmarks = nameFromBookmarks.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (nameFromBookmarks) {
        linkTitle = nameFromBookmarks.Value;
      }
      else if (xferData.length >= 2)
        linkTitle = xferData[1]
      else
        {
          // look up this URL's title in global history
          var potentialTitle = null;
          var historyDS = gRDFService.GetDataSource("rdf:history");
          var titlePropRes = RDFUtils.getResource(NC_RDF("Name"));
          var titleFromHistory = historyDS.GetTarget(elementRes, titlePropRes, true);
          if (titleFromHistory)
            titleFromHistory = titleFromHistory.QueryInterface(Components.interfaces.nsIRDFLiteral);
          if (titleFromHistory)
            potentialTitle = titleFromHistory.Value;
          linkTitle = potentialTitle;
        }

      var dropElement = aEvent.target.id;
      var dropElementRes, dropIndex, dropPosition;
      if (dropElement == "innermostBox") 
        {
          dropElementRes = personalToolbarRes;
          dropPosition = this.DROP_ON;
        }
      else
        {
          dropElementRes = RDFUtils.getResource(dropElement);
          rdfContainer.Init(childDB, personalToolbarRes);
          dropIndex = rdfContainer.IndexOf(dropElementRes);
          if (dropPosition == undefined)
            dropPosition = this.determineDropPosition(aEvent);
        }
      
      switch (dropPosition) {
      case this.DROP_BEFORE:
        if (dropIndex<1) dropIndex = 1;
        this.insertBookmarkAt(xferData[0], linkTitle, linkCharset, personalToolbarRes, dropIndex);
        break;
      case this.DROP_ON:
        this.insertBookmarkAt(xferData[0], linkTitle, linkCharset, dropElementRes, -1);
        break;
      case this.DROP_AFTER:
      default:
        // compensate for badly calculated dropIndex
        rdfContainer.Init(childDB, personalToolbarRes);
        if (dropIndex < rdfContainer.GetCount()) ++dropIndex;
        
        if (dropIndex<0) dropIndex = 0;
        this.insertBookmarkAt(xferData[0], linkTitle, linkCharset, personalToolbarRes, dropIndex);
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
       var dropPosition = this.determineDropPosition(aEvent);

      // bail if drop target is not a valid bookmark item or folder
      var inner = document.getElementById("innermostBox");
      if (aEvent.target.parentNode != inner && aEvent.target != inner) 
        {
          aDragSession.canDrop = false;
          return;
        }
      
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
          case this.DROP_BEFORE: 
            aEvent.target.setAttribute("dragover-left", "true");
            break;
          case this.DROP_AFTER:
            aEvent.target.setAttribute("dragover-right", "true");
            break;
          case this.DROP_ON:
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
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("moz/rdfitem");
      // application/x-moz-file
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    },

  determineDropPosition: function (aEvent)
    {
      var overButton = aEvent.target;
      var overButtonBoxObject = overButton.boxObject.QueryInterface(Components.interfaces.nsIBoxObject);

      // most things only drop on the left or right
      var regionCount = 2;

      // you can drop ONTO containers, so there is a "middle" region
      if (overButton.getAttribute("container") == "true" &&
          overButton.getAttribute("type") == "http://home.netscape.com/NC-rdf#Folder")
         return this.DROP_ON;

      var regionWidth = overButtonBoxObject.width/regionCount;

      // make sure we are vertically aligned with the button!
      if (aEvent.clientY < overButtonBoxObject.y ||
          aEvent.clientY > overButtonBoxObject.y + overButtonBoxObject.height)
        return 0;

      // in the first region?
      if (aEvent.clientX < (overButtonBoxObject.x + regionWidth))
          return this.DROP_BEFORE;

      // in the last region?
      if (aEvent.clientX >= (overButtonBoxObject.x + (regionCount - 1)*regionWidth))
          return this.DROP_AFTER;

      // must be in the middle somewhere
      return this.DROP_ON;
    },

  // returns the parent resource of the dragged element. This is determined
  // by inspecting the source element of the drag and walking up the DOM tree
  // to find the appropriate containing node.
  findParentContainer: function (aElement)
    {
      if (!aElement) return null;
      switch (aElement.localName) 
        {
          case "button":
          case "menubutton":
            var box = aElement.parentNode;
            return RDFUtils.getResource(box.getAttribute("ref"));
          case "menu":
          case "menuitem":
            var menu = aElement.parentNode.parentNode;
            if (menu.getAttribute("type") != "http://home.netscape.com/NC-rdf#Folder")
              return RDFUtils.getResource("NC:BookmarksRoot");
            return RDFUtils.getResource(menu.id);
          case "treecell":
            var treeitem = aElement.parentNode.parentNode.parentNode.parentNode;
            var res = treeitem.getAttribute("ref");
            if (!res)
              res = treeitem.id;            
            return RDFUtils.getResource(res);
        }
      return null;
    },
  
  insertBookmarkAt: function(aURL, aTitle, aCharset, aFolderRes, aIndex)
    {
      const kBMSContractID = "@mozilla.org/browser/bookmarks-service;1";
      const kBMSIID = Components.interfaces.nsIBookmarksService;
      const kBMS = Components.classes[kBMSContractID].getService(kBMSIID);
      kBMS.insertBookmarkInFolder(aURL, aTitle, aCharset, aFolderRes, aIndex);
    }
};

var proxyIconDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var urlBar = document.getElementById("urlbar");

      // XXX - do we want to allow the user to set a blank page to their homepage?
      //       if so then we want to modify this a little to set about:blank as
      //       the homepage in the event of an empty urlbar.
      if (!urlBar.value) return;

      var urlString = urlBar.value + "\n" + window._content.document.title;
      var htmlString = "<a href=\"" + urlBar.value + "\">" + urlBar.value + "</a>";

      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/x-moz-url", urlString);
      aXferData.data.addDataForFlavour("text/unicode", urlBar.value);
      aXferData.data.addDataForFlavour("text/html", htmlString);
    }
};

var homeButtonObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      var homepage = nsPreferences.getLocalizedUnicharPref("browser.startup.homepage", "about:blank");

      if (homepage)
        {
          // XXX find a readable title string for homepage, perhaps do a history lookup.
          var htmlString = "<a href=\"" + homepage + "\">" + homepage + "</a>";
          aXferData.data = new TransferData();
          aXferData.data.addDataForFlavour("text/x-moz-url", homepage + "\n" + homepage);
          aXferData.data.addDataForFlavour("text/html", htmlString);
          aXferData.data.addDataForFlavour("text/unicode", homepage);
        }
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
      var pressedVal = { };
      var promptTitle = gNavigatorBundle.getString("droponhometitle");
      var promptMsg   = gNavigatorBundle.getString("droponhomemsg");
      var okButton    = gNavigatorBundle.getString("droponhomeokbutton");

      promptService.confirmEx(window, promptTitle, promptMsg,
                              (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) +
                              (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1),
                              okButton, null, null, null, {value:0}, pressedVal);

      if (pressedVal.value == 0) {
        nsPreferences.setUnicharPref("browser.startup.homepage", url);
        setTooltipText("home-button", url);
      }
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = gNavigatorBundle.getString("droponhomebutton");
      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDragExit: function (aEvent, aDragSession)
    {
      var statusTextFld = document.getElementById("statusbar-display");
      statusTextFld.label = "";
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    }
};
