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
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

////////////////////////////////////////////////////////////////////////////
// XXX - WARNING - DRAG AND DROP API CHANGE ALERT - XXX
// This file has been extensively modified in a checkin planned for Mozilla
// 0.8, and the API has been modified. DO NOT MODIFY THIS FILE without 
// approval from ben@netscape.com, otherwise your changes will be lost. 

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
 * USAGE INFORMATION: see 'README-nsDragAndDrop.html' in the same source directory
 *                    as this file (typically xpfe/global/resources/content)
 */

var nsDragAndDrop = {
  
  _mDS: null,
  get mDragService()
    {
      if (!this._mDS) 
        {
          const kDSContractID = "@mozilla.org/widget/dragservice;1";
          const kDSIID = Components.interfaces.nsIDragService;
          this._mDS = Components.classes[kDSContractID].getService(kDSIID);
        }
      return this._mDS;
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
      if (!("onDragStart" in aDragDropObserver))
        return;

      const kDSIID = Components.interfaces.nsIDragService;
      var dragAction = { action: kDSIID.DRAGDROP_ACTION_COPY + kDSIID.DRAGDROP_ACTION_MOVE + kDSIID.DRAGDROP_ACTION_LINK };

      var transferData = { data: null };
      try 
        {
          aDragDropObserver.onDragStart(aEvent, transferData, dragAction);
        }
      catch (e) 
        {
          return;  // not a draggable item, bail!
        }

      if (!transferData.data) return;
      transferData = transferData.data;
      
      var transArray = nsJSSupportsUtils.createSupportsArray();
      var count = 0;
      do 
        {
          var trans = nsTransferable.set(transferData._XferID == "TransferData" 
                                         ? transferData 
                                         : transferData.dataList[count++]);
          transArray.AppendElement(trans.QueryInterface(Components.interfaces.nsISupports));
        }
      while (transferData._XferID == "TransferDataSet" && 
             count < transferData.dataList.length);
      
      this.mDragService.invokeDragSession(aEvent.target, transArray, null, dragAction.action);
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
      if (!("onDragOver" in aDragDropObserver)) 
        return;
      if (!this.mDragSession) 
        this.mDragSession = this.mDragService.getCurrentSession();
      if (this.mDragSession)
        {
          var flavourSet = aDragDropObserver.getSupportedFlavours();
          for (var flavour in flavourSet.flavourTable)
            {
              if (this.mDragSession.isDataFlavorSupported(flavour))
                {
                  this.mDragSession.canDrop = true;
                  aDragDropObserver.onDragOver(aEvent, 
                                               flavourSet.flavourTable[flavour], 
                                               this.mDragSession);
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
      if (!("onDrop" in aDragDropObserver))
        return;
        
      if (!this.mDragSession) 
        this.mDragSession = this.mDragService.getCurrentSession();
      if (this.mDragSession)
        {
          var flavourSet = aDragDropObserver.getSupportedFlavours();
          var transferData = nsTransferable.get(flavourSet, this.getDragData, true);
          aEvent.preventBubble();
          // hand over to the client to respond to dropped data
          var multiple = "canHandleMultipleItems" in aDragDropObserver && aDragDropObserver.canHandleMultipleItems;
          var dropData = multiple ? transferData : transferData.first.first;
          aDragDropObserver.onDrop(aEvent, dropData, this.mDragSession);
        }
    },

  dragExit: function (aEvent, aDragDropObserver)
    {
      if ("onDragExit" in aDragDropObserver)
        aDragDropObserver.onDragExit(aEvent, this.mDragSession);
    },  
    
  /** 
   * nsISupportsArray getDragData (Object aFlavourList)
   *
   * Creates a nsISupportsArray of all droppable items for the given
   * set of supported flavours.
   * 
   * @param FlavourSet aFlavourSet
   *        formatted flavour list.
   **/  
  getDragData: function (aFlavourSet)
    {
      var supportsArray = nsJSSupportsUtils.createSupportsArray();
      for (var i = 0; i < nsDragAndDrop.mDragSession.numDropItems; ++i)
        {
          var trans = nsTransferable.createTransferable();
          for (var j = 0; j < aFlavourSet.flavours.length; ++j)
            trans.addDataFlavor(aFlavourSet.flavours[j].contentType);
          nsDragAndDrop.mDragSession.getData(trans, i);
          supportsArray.AppendElement(trans);
        }
      return supportsArray;
    }

};

