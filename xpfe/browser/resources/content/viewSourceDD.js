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
 */

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
        throw Components.results.NS_ERROR_FAILURE;
      }
      
      var flavourList = { };
      flavourList["text/html"] = { width: 2, data: htmlstring };
      flavourList["text/unicode"] = { width: 2, data: textstring };
      return flavourList;
    },

  onDragOver: function (aEvent, aFlavour, aDragSession)
    {
      aDragSession.canDrop = false;
    },

  onDrop: function (aEvent, aData, aDragSession)
    {
        // can't accept drops
    },

  getSupportedFlavours: function ()
    {
      var flavourList = { };
      flavourList["text/unicode"] = { width: 2, iid: "nsISupportsWString" };
      return flavourList;
    }

};
