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
      // under the assumption that content areas won't contain
      // draggable XBL, we'll ignore the drag if we're dragging XBL
      // anonymous content nodes, like scrollbars, etc.
      // XXX bogus
      if (aEvent.target != aEvent.originalTarget)
        throw Components.results.NS_ERROR_FAILURE;

      // only drag form elements by using the alt key,
      // otherwise buttons and select widgets are hard to use
      if ('form' in aEvent.target && !aEvent.altKey)
        throw Components.results.NS_ERROR_FAILURE;

      var draggedNode = aEvent.target;

      // the resulting strings from the beginning of the drag
      var textstring = null;
      var urlstring = null;
      // htmlstring will be filled automatically if you fill urlstring
      var htmlstring = null;

      var domselection = window._content.getSelection();
      if (domselection && !domselection.isCollapsed && 
          domselection.containsNode(draggedNode,false))
        {
          var privateSelection = domselection.QueryInterface(Components.interfaces.nsISelectionPrivate);
          if (privateSelection)
          {
            // the window has a selection so we should grab that rather
            // than looking for specific elements
            htmlstring = privateSelection.toStringWithFormat("text/html", 128+256, 0);
            textstring = privateSelection.toStringWithFormat("text/plain", 0, 0);
            // how are we going to get the URL, if any? Scan the selection
            // for the first anchor? See bug #58315
          }
        }
      else 
        {
          if (aEvent.altKey && findParentNode(draggedNode, 'a'))
            return false;
          switch (draggedNode.localName.toUpperCase())
            {
              case 'AREA':
              case 'IMG':
                var imgsrc = draggedNode.getAttribute("src");
                var baseurl = window._content.location.href;
                // need to do some stuff with the window._content.location
                // (path?) to get base URL for image.

                // use alt text as the title of the image, if  it's there
                textstring = draggedNode.getAttribute("alt");
                urlstring = imgsrc;
                htmlstring = "<img src=\"" + urlstring + "\">";

                // if the image is also a link, then re-wrap htmlstring in
                // an anchor tag
                linkNode = findParentNode(draggedNode, 'a');
                if (linkNode) {
                  urlstring = this.getAnchorUrl(linkNode);
                  htmlstring = this.createLinkText(urlstring, htmlstring);
                }
                break;
              case 'A':
                urlstring = this.getAnchorUrl(draggedNode);
                textstring = this.getNodeString(draggedNode);
                aDragAction.action = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
                break;
                
              default:
                var linkNode = findParentNode(draggedNode, 'a');
                
                if (linkNode) {
                  urlstring = this.getAnchorUrl(linkNode);
                  textstring = this.getNodeString(linkNode);
                  
                  // select node now!
                  // this shouldn't be fatal, and
                  // we should still do the drag if this fails
                  try {
                    //this.normalizeSelection(linkNode, domselection);
                  } catch (ex) {
                    // non-fatal, so catch & ignore
                  }
  
                  aDragAction.action = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
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
      if (!textstring) textstring = urlstring;

      // if we haven't constructed a html version, make one now
      if (!htmlstring && urlstring)
          htmlstring = this.createLinkText(urlstring, urlstring);

      // now create the flavour lists
      aXferData.data = new TransferData();
      aXferData.data.addDataForFlavour("text/x-moz-url", urlstring + "\n" + textstring);
      aXferData.data.addDataForFlavour("text/html", htmlstring);
      aXferData.data.addDataForFlavour("text/unicode", textstring);
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      // if the drag originated w/in this content area, bail early. This avoids loading
      // a URL dragged from the content area into the very same content area (which is
      // almost never the desired action). This code is a bit too simplistic and may
      // have problems with nested frames, but that's not my problem ;)
      if (aDragSession.sourceDocument == window._content.document) {
        aDragSession.canDrop = false;
        return;
      }

      aDragSession.dragAction = Components.interfaces.nsIDragService.DRAGDROP_ACTION_LINK;
    },

  onDrop: function (aEvent, aXferData, aDragSession)
    {
      var url = retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);

      // valid urls don't contain spaces ' '; if we have a space it isn't a valid url so bail out
      if (!url || !url.length || url.indexOf(" ", 0) != -1) 
        return;

      switch (document.firstChild.getAttribute('windowtype')) {
        case "navigator:browser":
          loadShortcutOrURI(url);
          break;
        case "navigator:view-source":
          viewSource(url);
          break;
      }
    },

  getSupportedFlavours: function ()
    {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      flavourSet.appendFlavour("text/unicode");
      return flavourSet;
    },

  createLinkText: function(url, text)
  {
    return "<a href=\"" + url + "\">" + text + "</a>";
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


