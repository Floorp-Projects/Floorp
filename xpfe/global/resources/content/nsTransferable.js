/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

////////////////////////////////////////////////////////////////////////////
// XXX - WARNING - DRAG AND DROP API CHANGE ALERT - XXX
// This file has been extensively modified in a checkin planned for Mozilla
// 0.8, and the API has been modified. DO NOT MODIFY THIS FILE without 
// approval from ben@netscape.com, otherwise your changes will be lost. 

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
      return Components.classes[kXferableContractID].createInstance(kXferableIID);
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
        this.flavour.dataIIDKey != "nsISupportsString" )
      return this.supports.QueryInterface(Components.interfaces[this.flavour.dataIIDKey]); 
    else {
      var unicode = this.supports.QueryInterface(Components.interfaces.nsISupportsString);
      if (unicode) 
        return unicode.data.substring(0, this.contentLength/2);
     
      return this.supports;
    }
    return "";
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
        return aData;
      case "text/x-moz-url":
        return aData.toString().split("\n")[0];
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

/*
function DUMP_obj (aObj) 
{
  for (var i in aObj)
    dump("*** aObj[" + i + "] = " + aObj[i] + "\n");
}
*/

