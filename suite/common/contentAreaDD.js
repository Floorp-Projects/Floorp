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
 * Contributors:
 * 
 *   Alec Flett      <alecf@netscape.com>
 *   Ben Goodger     <ben@netscape.com>
 *   Mike Pinkerton  <pinkerton@netscape.com>
 *   Blake Ross      <blakeross@telocity.com>
 *   
 */

var contentAreaDNDObserver = {
  onDragStart: function (aEvent, aXferData, aDragAction)
    {
      if (aEvent.getPreventDefault())
        throw Components.results.NS_ERROR_FAILURE;

      // only drag form elements by using the alt key,
      // otherwise buttons and select widgets are hard to use
      if ('form' in aEvent.target && !aEvent.altKey)
        throw Components.results.NS_ERROR_FAILURE;

      var draggedNode = aEvent.target;

      // the resulting strings from the beginning of the drag
      var titlestring = null;
      var urlstring = null;
      // htmlstring will be filled automatically if you fill urlstring
      var htmlstring = null;

      var domselection = aEvent.view.getSelection();
      if (domselection && !domselection.isCollapsed && 
          domselection.containsNode(draggedNode,false))
        {
          // track down the anchor node, if any

          var firstAnchor = this.findFirstAnchor(domselection.anchorNode);

          if (firstAnchor)
            urlstring = firstAnchor.href;
          
          var privateSelection = domselection.QueryInterface(Components.interfaces.nsISelectionPrivate);
          if (privateSelection)
          {
          
            // the window has a selection so we should grab that rather
            // than looking for specific elements
            htmlstring = privateSelection.toStringWithFormat("text/html", 128+256, 0);
            titlestring = privateSelection.toStringWithFormat("text/plain", 0, 0);
          } else {
            titlestring = domselection.toString();
          }
        }
      else 
        {
          if (aEvent.altKey && findParentNode(draggedNode, 'a'))
            return false;
          
          var isAnchor = false;
          switch (draggedNode.localName.toUpperCase())
            {
              case 'AREA':
              case 'IMG':
                var imgsrc = draggedNode.getAttribute("src");
                var baseurl = window._content.location.href;
                // need to do some stuff with the window._content.location
                // (path?) to get base URL for image.

                // use alt text as the title of the image, if  it's there
                titlestring = draggedNode.getAttribute("alt");
                urlstring = imgsrc;
                htmlstring = "<img src=\"" + urlstring + "\">";

                // if the image is also a link, then re-wrap htmlstring in
                // an anchor tag
                linkNode = findParentNode(draggedNode, 'a');
                if (linkNode) {
                  isAnchor = true;
                  urlstring = this.getAnchorUrl(linkNode);
                  htmlstring = this.createLinkText(urlstring, htmlstring);
                }
                break;
              case 'A':
                urlstring = this.getAnchorUrl(draggedNode);
                titlestring = this.getNodeString(draggedNode);
                 // this causes d&d problems on windows -- see bug 68058
                 //aDragAction.action =  Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
                isAnchor = true;
                break;
                
              default:
                var linkNode = findParentNode(draggedNode, 'a');
                
                if (linkNode) {
                  urlstring = this.getAnchorUrl(linkNode);
                  titlestring = this.getNodeString(linkNode);
                  // select node now!
                  // this shouldn't be fatal, and
                  // we should still do the drag if this fails
                  try {
                    this.normalizeSelection(linkNode, domselection);
                  } catch (ex) {
                    // non-fatal, so catch & ignore
                  }
  
                 isAnchor = true;
                 // this causes d&d problems on windows -- see bug 68058
                 //aDragAction.action = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
                }
                else {
                  // Need to throw to indicate that the drag target should not 
                  // allow drags.
                  throw Components.results.NS_OK;
                }
                break;
            }
        }

      // default text value is the URL
      if (!titlestring) titlestring = urlstring;

      // if we haven't constructed a html version, make one now
      if (!htmlstring && urlstring)
          htmlstring = this.createLinkText(urlstring, urlstring);

      // now create the flavour lists
      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/unicode", isAnchor ? urlstring :  titlestring);
      aXferData.data.addDataForFlavour("text/html", htmlstring);
      if (urlstring) {
        aXferData.data.addDataForFlavour("text/x-moz-url", urlstring + "\n" + titlestring);
        aXferData.data.addDataForFlavour("moz/rdfitem", urlstring + "\n" + titlestring);
      }
      else {
        aXferData.data.addDataForFlavour("moz/rdfitem", titlestring);
      }
      // we use the url for text/unicode data if an anchor is being dragged, rather than
      // the title text of the link or the alt text for an anchor image. 
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      if (aEvent.getPreventDefault())
        return;
        
      // if the drag originated w/in this content area, bail
      // early. This avoids loading a URL dragged from the content
      // area into the very same content area (which is almost never
      // the desired action). This code is a bit too simplistic and
      // may have problems with nested frames, but that's not my
      // problem ;)
      if (aDragSession.sourceDocument == aEvent.view.document) {
        aDragSession.canDrop = false;
        return;
      }
      // this causes d&d problems on windows -- see bug 68058
      //aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);

      // valid urls don't contain spaces ' '; if we have a space it isn't a valid url so bail out
      if (!url || !url.length || url.indexOf(" ", 0) != -1) 
        return;

      switch (document.firstChild.getAttribute('windowtype')) {
        case "navigator:browser":
          loadURI(getShortcutOrURI(url));
          break;
        case "navigator:view-source":
          viewSource(url);
          break;
      }
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
    },

  createLinkText: function(url, text)
  {
    return "<a href=\"" + url + "\">" + text + "</a>";
  },

  findFirstAnchor: function(node)
  {
    if (!node) return null;

    while (node) {
      if (node.nodeType == Node.ELEMENT_NODE &&
          node.localName.toLowerCase() == "a")
        return node;
      
      var childResult = this.findFirstAnchor(node.firstChild);
      if (childResult)
        return childResult;

      node = node.nextSibling;
    }
    return null;
  },
  
  normalizeSelection: function(baseNode, domselection)
  {
    var parent = baseNode.parentNode;
    if (!parent) return;
    if (!domselection) return;

    var nodelist = parent.childNodes;
    var index;
    for (index = 0; index<nodelist.length; index++)
    {
      if (nodelist.item(index) == baseNode)
        break;
    }

    if (index >= nodelist.length) {
      throw Components.results.NS_ERROR_FAILURE;
    }

    // now make the selection contain all of the parent's children up to
    // the selected one
    domselection.collapse(parent,index);
    domselection.extend(parent,index+1);
  },

  getAnchorUrl: function(linkNode)
  {
    return linkNode.href || linkNode.name || null;
  },

  getNodeString: function(node)
  {
    // use a range to get the text-equivalent of the node
    var range = document.createRange();
    range.selectNode(node);
    return range.toString();
  }
  
};


function retrieveURLFromData (aData, flavour)
{
  switch (flavour) {
  case "text/unicode":
    // this might not be a url, but we'll return it anyway
    return aData;
  case "text/x-moz-url":
    var data = aData.toString();
    var separator = data.indexOf("\n");
    if (separator != -1)
      data = data.substr(0, separator);
    return data;
  case "application/x-moz-file":
    const kURLContractID = "@mozilla.org/network/standard-url;1";
    const kFileURLIID = Components.interfaces.nsIFileURL;
    var fileURL = Components.classes[kURLContractID].createInstance(kFileURLIID);
    fileURL.file = aData;
    return fileURL.spec;
  }             
  return null;                                                         
}


