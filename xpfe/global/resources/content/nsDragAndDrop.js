/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author:
 *   Ben Matthew Goodger <ben@netscape.com>
 *
 * Contributor(s): 
 */

/**
 * XXX - until load is supported in chrome, you also need to include 
 *       these files:
 *       chrome://global/content/nsJSSupportsUtils.js
 *       chrome://global/content/nsJSComponentManager.js
 *       chrome://global/content/nsTransferable.js
 **/



/**
 * nsDragAndDrop - a convenience wrapper for nsTransferable, nsITransferable
 *                 and nsIDragService/nsIDragSession. 
 *
 * Use: map the handler functions to the 'ondraggesture', 'ondragover' and 
 *      'ondragdrop' event handlers on your XML element, e.g.
 *      <xmlelement ondraggesture="nsDragAndDrop.startDrag(event, observer);"
 *                  ondragover="nsDragAndDrop.startDrag(event, observer);"
 *                  ondragdrop="nsDragAndDrop.drop(event, observer);"/>
 *
 *      You need to create an observer js object with the following member
 *      functions:
 *        Object onDragStart (event)        // called when drag initiated, 
 *                                          // returns flavour list with data
 *                                          // to stuff into transferable
 *        void onDragOver (Object flavour)  // called when element is dragged
 *                                          // over, so that it can perform
 *                                          // any drag-over feedback for provided
 *                                          // flavour
 *        void onDrop (Object data)         // formatted data object dropped.
 *        Object getSupportedFlavours ()    // returns a flavour list so that
 *                                          // nsTransferable can determine whether
 *                                          // or not to accept drop.
 **/ 

var nsDragAndDrop = {

  get mDragService()
    {
      return nsJSComponentManager.getService("@mozilla.org/widget/dragservice;1",
                                             "nsIDragService");
    },

  /**
   * void startDrag (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when a drag on an element is started.
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by the drag init
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/  
  startDrag: function (aEvent, aDragDropObserver)
    {
      var flavourList = null;

      if (aDragDropObserver.onDragStart)
        {
          try 
            {
              flavourList = aDragDropObserver.onDragStart(aEvent);
            }
          catch (e)
            {
              return; // not a draggable item, bail!
            }
        }

      if (! flavourList || flavourList.length < 1)
        return;

      var trans = nsTransferable.set(flavourList);
      trans = trans ? trans.QueryInterface(Components.interfaces.nsISupports) : trans;
      var transArray = nsJSSupportsUtils.createSupportsArray();
      transArray.AppendElement(trans);
      
      var dragServiceIID = Components.interfaces.nsIDragService;
      this.mDragService.invokeDragSession(aEvent.target, transArray, null, 
                                          dragServiceIID.DRAGDROP_ACTION_COPY + dragServiceIID.DRAGDROP_ACTION_MOVE + dragServiceIID.DRAGDROP_ACTION_LINK);
      aEvent.preventBubble();
    },

  /** 
   * void dragOver (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when a drag passes over this element
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by the drag init
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  dragOver: function (aEvent, aDragDropObserver)
    { 
      if (!this.mDragSession) 
        this.mDragSession = this.mDragService.getCurrentSession();
      if (this.mDragSession)
        {
          var flavourList = aDragDropObserver.getSupportedFlavours();
          for (var flavour in flavourList)
            {
              if (this.mDragSession.isDataFlavorSupported(flavour))
                {
                  this.mDragSession.canDrop = true;
                  if (aDragDropObserver.onDragOver)
                    aDragDropObserver.onDragOver(aEvent, flavour, this.mDragSession);
                  aEvent.preventBubble();
                  break;
                }
            }
        }
    },

  mDragSession: null,

  /** 
   * void drop (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when the user drops on the element
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by the drag init
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  drop: function (aEvent, aDragDropObserver)
    {
      if (!this.mDragSession) 
        this.mDragSession = this.mDragService.getCurrentSession();
      if (this.mDragSession)
        {
          var flavourList = aDragDropObserver.getSupportedFlavours();
          var dragData = nsTransferable.get(flavourList, this.getDragData, true);
          aEvent.preventBubble();
          // hand over to the client to respond to dropped data
          if (aDragDropObserver.onDrop) 
            aDragDropObserver.onDrop(aEvent, dragData, this.mDragSession);
        }
    },

  dragExit: function (aEvent, aDragDropObserver)
    {
      if (aDragDropObserver.onDragExit)
        aDragDropObserver.onDragExit(aEvent, this.mDragSession);
    },  
    
  /** 
   * nsISupportsArray getDragData (Object aFlavourList)
   *
   * Creates a nsISupportsArray of all droppable items for the given
   * set of supported flavours.
   * 
   * @param Object aFlavourList
   *        formatted flavour list.
   **/  
  getDragData: function (aFlavourList)
    {
      var supportsArray = nsJSSupportsUtils.createSupportsArray();
      for (var i = 0; i < nsDragAndDrop.mDragSession.numDropItems; ++i)
        {
          var trans = nsTransferable.createTransferable();
          for (var flavour in aFlavourList)
            trans.addDataFlavor(flavour);
          nsDragAndDrop.mDragSession.getData(trans, i);
          supportsArray.AppendElement(trans);
        }
      return supportsArray;
    }

};

