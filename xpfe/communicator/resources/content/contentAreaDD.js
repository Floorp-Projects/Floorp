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

var gSourceDocument, wasDrag;
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
          if (aEvent.altKey && this.findEnclosingLink(aEvent.target))
            return false;
          switch (aEvent.originalTarget.localName) 
            {
              case '#text':
                var node = this.findEnclosingLink(aEvent.target);
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
              case 'LI':
              case 'OL':
              case 'DD':
              default:
                var node = this.findEnclosingLink(aEvent.target);
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
        flavourList["text/x-moz-url"] = { width: 2, data: textstring };
      flavourList["text/unicode"] = { width: 2, data: textstring };
      return flavourList;
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      // if the drag originated w/in this content area, bail early. This avoids loading
      // a URL dragged from the content area into the very same content area (which is
      // almost never the desired action). This code is a bit too simplistic and may
      // have problems with nested frames, but that's not my problem ;)
      if (aDragSession.sourceDocument == window._content.document)
        aDragSession.canDrop = false;
    },

  onDrop: function (aEvent, aData, aDragSession)
    {
      var data = aData.length ? aData[0] : aData;
      var url = retrieveURLFromData(data);
      if (url.length == 0)
        return true;
      // valid urls don't contain spaces ' '; if we have a space it isn't a valid url so bail out
      var urlstr = url.toString();
      if ( urlstr.indexOf(" ", 0) != -1 )
        return true;
      switch (document.firstChild.getAttribute('windowtype')) {
        case "navigator:browser":
          var urlBar = document.getElementById("urlbar");
          urlBar.value = url;
          BrowserLoadURL();
          break;
        case "navigator:view-source":
          viewSource(url);
          break;
        default:
      }
    },

  getSupportedFlavours: function ()
    {
      var flavourList = { };
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
        if (nodeName == "a")
          return aNode;
        aNode = aNode.parentNode;
      }
      return null;
    }
};


function retrieveURLFromData (aData)
{
  switch (aData.flavour) {
    case "text/unicode":
    case "text/x-moz-url":
      return aData.data.data; // XXX this is busted. 
      break;
    case "application/x-moz-file":
      var dataObj = aData.data.data.QueryInterface(Components.interfaces.nsIFile);
      if (dataObj) {
        var fileURL = nsJSComponentManager.createInstance("@mozilla.org/network/standard-url;1",
                                                           "nsIFileURL");
        fileURL.file = dataObj;
        return fileURL.spec;
      }
  }             
  return null;                                                         
}
