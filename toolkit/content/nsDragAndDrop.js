// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

////////////////////////////////////////////////////////////////////////
//
// USE OF THIS API FOR DRAG AND DROP IS DEPRECATED!
// Do not use this file for new code.
//
// For documentation about what to use instead, see:
//   http://developer.mozilla.org/En/DragDrop/Drag_and_Drop
//
////////////////////////////////////////////////////////////////////////


/** 
 *  nsTransferable - a wrapper for nsITransferable that simplifies
 *                   javascript clipboard and drag&drop. for use in
 *                   these situations you should use the nsClipboard
 *                   and nsDragAndDrop wrappers for more convenience
 **/ 
 
var nsTransferable = {
  /**
   * nsITransferable set (TransferData aTransferData) ;
   *
   * Creates a transferable with data for a list of supported types ("flavours")
   * 
   * @param TransferData aTransferData
   *        a javascript object in the format described above 
   **/ 
  set: function (aTransferDataSet)
    {
      var trans = this.createTransferable();
      for (var i = 0; i < aTransferDataSet.dataList.length; ++i) 
        {
          var currData = aTransferDataSet.dataList[i];
          var currFlavour = currData.flavour.contentType;
          trans.addDataFlavor(currFlavour);
          var supports = null; // nsISupports data
          var length = 0;
          if (currData.flavour.dataIIDKey == "nsISupportsString")
            {
              supports = Components.classes["@mozilla.org/supports-string;1"]
                                   .createInstance(Components.interfaces.nsISupportsString);

              supports.data = currData.supports;
              length = supports.data.length;
            }
          else 
            {
              // non-string data.
              supports = currData.supports;
              length = 0; // kFlavorHasDataProvider
            }
          trans.setTransferData(currFlavour, supports, length * 2);
        }
      return trans;
    },
  
  /**
   * TransferData/TransferDataSet get (FlavourSet aFlavourSet, 
   *                                   Function aRetrievalFunc, Boolean aAnyFlag) ;
   *
   * Retrieves data from the transferable provided in aRetrievalFunc, formatted
   * for more convenient access.
   *
   * @param FlavourSet aFlavourSet
   *        a FlavourSet object that contains a list of supported flavours.
   * @param Function aRetrievalFunc
   *        a reference to a function that returns a nsISupportsArray of nsITransferables
   *        for each item from the specified source (clipboard/drag&drop etc)
   * @param Boolean aAnyFlag
   *        a flag specifying whether or not a specific flavour is requested. If false,
   *        data of the type of the first flavour in the flavourlist parameter is returned,
   *        otherwise the best flavour supported will be returned.
   **/
  get: function (aFlavourSet, aRetrievalFunc, aAnyFlag)
    {
      if (!aRetrievalFunc) 
        throw "No data retrieval handler provided!";
      
      var supportsArray = aRetrievalFunc(aFlavourSet);
      var dataArray = [];
      var count = supportsArray.Count();
      
      // Iterate over the number of items returned from aRetrievalFunc. For
      // clipboard operations, this is 1, for drag and drop (where multiple
      // items may have been dragged) this could be >1.
      for (var i = 0; i < count; i++)
        {
          var trans = supportsArray.GetElementAt(i);
          if (!trans) continue;
          trans = trans.QueryInterface(Components.interfaces.nsITransferable);
            
          var data = { };
          var length = { };
          
          var currData = null;
          if (aAnyFlag)
            { 
              var flavour = { };
              trans.getAnyTransferData(flavour, data, length);
              if (data && flavour)
                {
                  var selectedFlavour = aFlavourSet.flavourTable[flavour.value];
                  if (selectedFlavour) 
                    dataArray[i] = FlavourToXfer(data.value, length.value, selectedFlavour);
                }
            }
          else
            {
              var firstFlavour = aFlavourSet.flavours[0];
              trans.getTransferData(firstFlavour, data, length);
              if (data && firstFlavour)
                dataArray[i] = FlavourToXfer(data.value, length.value, firstFlavour);
            }
        }
      return new TransferDataSet(dataArray);
    },

  /** 
   * nsITransferable createTransferable (void) ;
   *
   * Creates and returns a transferable object.
   **/    
  createTransferable: function ()
    {
      const kXferableContractID = "@mozilla.org/widget/transferable;1";
      const kXferableIID = Components.interfaces.nsITransferable;
      var trans = Components.classes[kXferableContractID].createInstance(kXferableIID);
      trans.init(null);
      return trans;
    }
};  

/** 
 * A FlavourSet is a simple type that represents a collection of Flavour objects.
 * FlavourSet is constructed from an array of Flavours, and stores this list as
 * an array and a hashtable. The rationale for the dual storage is as follows:
 * 
 * Array: Ordering is important when adding data flavours to a transferable. 
 *        Flavours added first are deemed to be 'preferred' by the client. 
 * Hash:  Convenient lookup of flavour data using the content type (MIME type)
 *        of data as a key. 
 */
function FlavourSet(aFlavourList)
{
  this.flavours = aFlavourList || [];
  this.flavourTable = { };

  this._XferID = "FlavourSet";
  
  for (var i = 0; i < this.flavours.length; ++i)
    this.flavourTable[this.flavours[i].contentType] = this.flavours[i];
}

FlavourSet.prototype = {
  appendFlavour: function (aFlavour, aFlavourIIDKey)
  {
    var flavour = new Flavour (aFlavour, aFlavourIIDKey);
    this.flavours.push(flavour);
    this.flavourTable[flavour.contentType] = flavour;
  }
};

/** 
 * A Flavour is a simple type that represents a data type that can be handled. 
 * It takes a content type (MIME type) which is used when storing data on the
 * system clipboard/drag and drop, and an IIDKey (string interface name
 * which is used to QI data to an appropriate form. The default interface is
 * assumed to be wide-string.
 */ 
function Flavour(aContentType, aDataIIDKey)
{
  this.contentType = aContentType;
  this.dataIIDKey = aDataIIDKey || "nsISupportsString";

  this._XferID = "Flavour";
}

function TransferDataBase() {}
TransferDataBase.prototype = {
  push: function (aItems)
  {
    this.dataList.push(aItems);
  },

  get first ()
  {
    return "dataList" in this && this.dataList.length ? this.dataList[0] : null;
  }
};

/** 
 * TransferDataSet is a list (array) of TransferData objects, which represents
 * data dragged from one or more elements. 
 */
function TransferDataSet(aTransferDataList)
{
  this.dataList = aTransferDataList || [];

  this._XferID = "TransferDataSet";
}
TransferDataSet.prototype = TransferDataBase.prototype;

/** 
 * TransferData is a list (array) of FlavourData for all the applicable content
 * types associated with a drag from a single item. 
 */
function TransferData(aFlavourDataList)
{
  this.dataList = aFlavourDataList || [];

  this._XferID = "TransferData";
}
TransferData.prototype = {
  __proto__: TransferDataBase.prototype,
  
  addDataForFlavour: function (aFlavourString, aData, aLength, aDataIIDKey)
  {
    this.dataList.push(new FlavourData(aData, aLength, 
                       new Flavour(aFlavourString, aDataIIDKey)));
  }
};

/** 
 * FlavourData is a type that represents data retrieved from the system 
 * clipboard or drag and drop. It is constructed internally by the Transferable
 * using the raw (nsISupports) data from the clipboard, the length of the data,
 * and an object of type Flavour representing the type. Clients implementing
 * IDragDropObserver receive an object of this type in their implementation of
 * onDrop. They access the 'data' property to retrieve data, which is either data 
 * QI'ed to a usable form, or unicode string. 
 */
function FlavourData(aData, aLength, aFlavour) 
{
  this.supports = aData;
  this.contentLength = aLength;
  this.flavour = aFlavour || null;
  
  this._XferID = "FlavourData";
}

FlavourData.prototype = {
  get data ()
  {
    if (this.flavour &&
        this.flavour.dataIIDKey != "nsISupportsString")
      return this.supports.QueryInterface(Components.interfaces[this.flavour.dataIIDKey]); 

    var supports = this.supports;
    if (supports instanceof Components.interfaces.nsISupportsString)
      return supports.data.substring(0, this.contentLength/2);
     
    return supports;
  }
}

/** 
 * Create a TransferData object with a single FlavourData entry. Used when 
 * unwrapping data of a specific flavour from the drag service. 
 */
function FlavourToXfer(aData, aLength, aFlavour) 
{
  return new TransferData([new FlavourData(aData, aLength, aFlavour)]);
}

var transferUtils = {

  retrieveURLFromData: function (aData, flavour)
  {
    switch (flavour) {
      case "text/unicode":
      case "text/plain":
      case "text/x-moz-text-internal":
        return aData.replace(/^\s+|\s+$/g, "");
      case "text/x-moz-url":
        return ((aData instanceof Components.interfaces.nsISupportsString) ? aData.toString() : aData).split("\n")[0];
      case "application/x-moz-file":
        var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                  .getService(Components.interfaces.nsIIOService);
        var fileHandler = ioService.getProtocolHandler("file")
                                   .QueryInterface(Components.interfaces.nsIFileProtocolHandler);
        return fileHandler.getURLSpecFromFile(aData);
    }
    return null;                                                   
  }

}

/**
 * nsDragAndDrop - a convenience wrapper for nsTransferable, nsITransferable
 *                 and nsIDragService/nsIDragSession. 
 *
 * Use: map the handler functions to the 'ondraggesture', 'ondragover' and
 *   'ondragdrop' event handlers on your XML element, e.g.                   
 *   <xmlelement ondraggesture="nsDragAndDrop.startDrag(event, observer);"   
 *               ondragover="nsDragAndDrop.dragOver(event, observer);"      
 *               ondragdrop="nsDragAndDrop.drop(event, observer);"/>         
 *                                                                           
 *   You need to create an observer js object with the following member      
 *   functions:                                                              
 *     Object onDragStart (event)        // called when drag initiated,      
 *                                       // returns flavour list with data   
 *                                       // to stuff into transferable      
 *     void onDragOver (Object flavour)  // called when element is dragged   
 *                                       // over, so that it can perform     
 *                                       // any drag-over feedback for provided
 *                                       // flavour                          
 *     void onDrop (Object data)         // formatted data object dropped.   
 *     Object getSupportedFlavours ()    // returns a flavour list so that   
 *                                       // nsTransferable can determine
 *                                       // whether or not to accept drop. 
 **/   

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

      var dt = aEvent.dataTransfer;
      var count = 0;
      do {
        var tds = transferData._XferID == "TransferData" 
                                         ? transferData 
                                         : transferData.dataList[count]
        for (var i = 0; i < tds.dataList.length; ++i) 
        {
          var currData = tds.dataList[i];
          var currFlavour = currData.flavour.contentType;
          var value = currData.supports;
          if (value instanceof Components.interfaces.nsISupportsString)
            value = value.toString();
          dt.mozSetDataAt(currFlavour, value, count);
        }

        count++;
      }
      while (transferData._XferID == "TransferDataSet" && 
             count < transferData.dataList.length);

      dt.effectAllowed = "all";
      // a drag targeted at a tree should instead use the treechildren so that
      // the current selection is used as the drag feedback
      dt.addElement(aEvent.originalTarget.localName == "treechildren" ?
                    aEvent.originalTarget : aEvent.target);
      aEvent.stopPropagation();
    },

  /** 
   * void dragOver (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when a drag passes over this element
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by passing over the element
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  dragOver: function (aEvent, aDragDropObserver)
    { 
      if (!("onDragOver" in aDragDropObserver)) 
        return;
      if (!this.checkCanDrop(aEvent, aDragDropObserver))
        return;
      var flavourSet = aDragDropObserver.getSupportedFlavours();
      for (var flavour in flavourSet.flavourTable)
        {
          if (this.mDragSession.isDataFlavorSupported(flavour))
            {
              aDragDropObserver.onDragOver(aEvent, 
                                           flavourSet.flavourTable[flavour], 
                                           this.mDragSession);
              aEvent.stopPropagation();
              aEvent.preventDefault();
              break;
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
   *        the DOM event fired by the drop
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  drop: function (aEvent, aDragDropObserver)
    {
      if (!("onDrop" in aDragDropObserver))
        return;
      if (!this.checkCanDrop(aEvent, aDragDropObserver))
        return;  

      var flavourSet = aDragDropObserver.getSupportedFlavours();

      var dt = aEvent.dataTransfer;
      var dataArray = [];
      var count = dt.mozItemCount;
      for (var i = 0; i < count; ++i) {
        var types = dt.mozTypesAt(i);
        for (var j = 0; j < flavourSet.flavours.length; j++) {
          var type = flavourSet.flavours[j].contentType;
          // dataTransfer uses text/plain but older code used text/unicode, so
          // switch this for compatibility
          var modtype = (type == "text/unicode") ? "text/plain" : type;
          if (Array.indexOf(types, modtype) >= 0) {
            var data = dt.mozGetDataAt(modtype, i);
            if (data) {
              // Non-strings need some non-zero value used for their data length.
              const kNonStringDataLength = 4;

              var length = (typeof data == "string") ? data.length : kNonStringDataLength;
              dataArray[i] = FlavourToXfer(data, length, flavourSet.flavourTable[type]);
              break;
            }
          }
        }
      }

      var transferData = new TransferDataSet(dataArray)

      // hand over to the client to respond to dropped data
      var multiple = "canHandleMultipleItems" in aDragDropObserver && aDragDropObserver.canHandleMultipleItems;
      var dropData = multiple ? transferData : transferData.first.first;
      aDragDropObserver.onDrop(aEvent, dropData, this.mDragSession);
      aEvent.stopPropagation();
    },

  /** 
   * void dragExit (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when a drag leaves this element
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by leaving the element
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  dragExit: function (aEvent, aDragDropObserver)
    {
      if (!this.checkCanDrop(aEvent, aDragDropObserver))
        return;
      if ("onDragExit" in aDragDropObserver)
        aDragDropObserver.onDragExit(aEvent, this.mDragSession);
    },  
    
  /** 
   * void dragEnter (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * called when a drag enters in this element
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by entering in the element
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  dragEnter: function (aEvent, aDragDropObserver)
    {
      if (!this.checkCanDrop(aEvent, aDragDropObserver))
        return;
      if ("onDragEnter" in aDragDropObserver)
        aDragDropObserver.onDragEnter(aEvent, this.mDragSession);
    },  

  /** 
   * Boolean checkCanDrop (DOMEvent aEvent, Object aDragDropObserver) ;
   *
   * Sets the canDrop attribute for the drag session.
   * returns false if there is no current drag session.
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by the drop
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   **/
  checkCanDrop: function (aEvent, aDragDropObserver)
    {
      if (!this.mDragSession) 
        this.mDragSession = this.mDragService.getCurrentSession();
      if (!this.mDragSession) 
        return false;
      this.mDragSession.canDrop = this.mDragSession.sourceNode != aEvent.target;
      if ("canDrop" in aDragDropObserver)
        this.mDragSession.canDrop &= aDragDropObserver.canDrop(aEvent, this.mDragSession);
      return true;
    },

  /**
   * Do a security check for drag n' drop. Make sure the source document
   * can load the dragged link.
   *
   * @param DOMEvent aEvent
   *        the DOM event fired by leaving the element
   * @param Object aDragDropObserver
   *        javascript object of format described above that specifies
   *        the way in which the element responds to drag events.
   * @param String aDraggedText
   *        the text being dragged
   **/
  dragDropSecurityCheck: function (aEvent, aDragSession, aDraggedText)
    {
      // Strip leading and trailing whitespace, then try to create a
      // URI from the dropped string. If that succeeds, we're
      // dropping a URI and we need to do a security check to make
      // sure the source document can load the dropped URI. We don't
      // so much care about creating the real URI here
      // (i.e. encoding differences etc don't matter), we just want
      // to know if aDraggedText really is a URI.

      aDraggedText = aDraggedText.replace(/^\s*|\s*$/g, '');

      var uri;
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      try {
        uri = ioService.newURI(aDraggedText, null, null);
      } catch (e) {
      }

      if (!uri)
        return;

      // aDraggedText is a URI, do the security check.
      const nsIScriptSecurityManager = Components.interfaces
                                                 .nsIScriptSecurityManager;
      var secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                             .getService(nsIScriptSecurityManager);

      if (!aDragSession)
        aDragSession = this.mDragService.getCurrentSession();

      var sourceDoc = aDragSession.sourceDocument;
      // Use "file:///" as the default sourceURI so that drops of file:// URIs
      // are always allowed.
      var principal = sourceDoc ? sourceDoc.nodePrincipal
                                : secMan.getSimpleCodebasePrincipal(ioService.newURI("file:///", null, null));

      try {
        secMan.checkLoadURIStrWithPrincipal(principal, aDraggedText,
                                            nsIScriptSecurityManager.STANDARD);
      } catch (e) {
        // Stop event propagation right here.
        aEvent.stopPropagation();

        throw "Drop of " + aDraggedText + " denied.";
      }
    }
};

