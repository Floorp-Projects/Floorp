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
 *  nsTransferable - a wrapper for nsITransferable that simplifies
 *                   javascript clipboard and drag&drop. for use in
 *                   these situations you should use the nsClipboard
 *                   and nsDragAndDrop wrappers for more convenience
 **/ 
 
var nsTransferable = {

  /** 
   * Flavour List Format:
   * flavourList["text/unicode"].width // width of data
   * flavourList["text/unicode"].iid   // iid of data type
   * flavourList["text/unicode"].data  // data to be stored (if any)
   **/

  /**
   * nsITransferable set (Object aFlavourList) ;
   *
   * Creates a transferable with data for a list of supported types ("flavours")
   * 
   * @param Object aFlavourList
   *        a javascript object in the format described above 
   **/ 
  set: function (aFlavourList)
    {
      var trans = this.createTransferable();
      for (var flavour in aFlavourList)
        {
          trans.addDataFlavor(flavour);
          var width = aFlavourList[flavour].width;
          var wrapper = this.createEmptyWrapper(width);
          if (wrapper) 
            {
              wrapper.data = aFlavourList[flavour].data;
              trans.setTransferData(flavour, wrapper, wrapper.data.length * width);
            }
        }
      return trans;
    },
  
  /**
   * Array/Object get (Object aFlavourList, Function aRetrievalFunc, Boolean aAnyFlag) ;
   *
   * Retrieves data from the transferable provided in aRetrievalFunc, formatted
   * for more convenient access.
   *
   * @param Object aFlavourList
   *        a javascript object in the format described above
   * @param Function aRetrievalFunc
   *        a reference to a function that returns a nsISupportsArray of nsITransferables
   *        for each item from the specified source (clipboard/drag&drop etc)
   * @param Boolean aAnyFlag
   *        a flag specifying whether or not a specific flavour is requested. If false,
   *        data of the type of the first flavour in the flavourlist parameter is returned,
   *        otherwise the best flavour supported will be returned.
   **/
  get: function (aFlavourList, aRetrievalFunc, aAnyFlag)
    {
      var firstFlavour = null;
      for (var flav in aFlavourList)
        {
          firstFlavour = flav;
          break;
        }
      
      if (aRetrievalFunc)
        { 
          var supportsArray = aRetrievalFunc(aFlavourList);
          var dataArray = [];
          for (var i = 0; i < supportsArray.Count(); i++)
            {
              trans = supportsArray.GetElementAt(i);
              if (trans) 
                trans = trans.QueryInterface(Components.interfaces.nsITransferable);
                
              var data = { };
              var flavour = { };
              var length = { };
              
              if (aAnyFlag)
                { 
                  trans.getAnyTransferData(flavour, data, length);
                  if (data && flavour)
                    {
                      var selectedFlavour = aFlavourList[flavour.value];
                      if (selectedFlavour)
                        {
                          data = data.value.QueryInterface(Components.interfaces[selectedFlavour.iid]);
                          var currData = 
                            {
                              data: { data: data, length: length.value, width: selectedFlavour.width }, // this.wrapData(data.value, length.value, selectedFlavour.width),
                              flavour: flavour.value
                            };
                          if (supportsArray.Count() == 1)
                            return currData;
                          else
                            dataArray[i] = currData;  
                        }
                    }
                  dataArray[i] = null;
                }
              else
                {
                  trans.getTransferData(firstFlavour, data, length);
                  var curData = data ? this.wrapData(data.value, length.value, aFlavourList[firstFlavour].width) : null;
                  if (supportsArray.Count() == 1)
                    return curData;
                  else
                    dataArray[i] = curData;
                }
            }
          return dataArray;
        }
      else
        throw "No data retrieval handler provided!";
        
      return null;      // quiet warnings
    },

  /** 
   * nsITransferable createTransferable (void) ;
   *
   * Creates and returns a transferable object.
   **/    
  createTransferable: function ()
    {
      return nsJSComponentManager.createInstance("@mozilla.org/widget/transferable;1",
                                                 "nsITransferable");
    },

  /**
   * nsISupports createEmptyWrapper (int aWidth) ;
   *
   * Creates a wrapper for string data. XXX - this is bad, we're assuming the data we're
   * dragging is string.
   *
   * @param int aWidth  
   *        the width of the data (single or double byte)
   **/
  createEmptyWrapper: function (aWidth)
    {
      return aWidth == 2 ? nsJSSupportsUtils.createSupportsWString() : 
                           nsJSSupportsUtils.createSupportsString();

    },
  
  /**
   * String wrapData (Object aDataObject, int aLength, int aWidth)
   *
   * Returns the actual string representation of nsISupports[W]String wrapped
   * data.
   *
   * @param Object aDataObject
   *        the data to be unwrapped
   * @param int aLength
   *        the integer length of the data 
   * @param int aWidth
   *        the unit size
   **/
  wrapData: function (aDataObject, aLength, aWidth)
    {
      const IID = aWidth == 2 ? Components.interfaces.nsISupportsWString :
                                Components.interfaces.nsISupportsString;
      var data = aDataObject.QueryInterface(IID);
      if (data)
        return data.data.substring(0, aLength / aWidth);
      
      return null;
    }
};  
