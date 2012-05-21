/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** 
 * nsClipboard - wrapper around nsIClipboard and nsITransferable
 *               that simplifies access to the clipboard. 
 **/ 
var nsClipboard = {
  _CB: null,
  get mClipboard()
    {
      if (!this._CB) 
        {
          const kCBContractID = "@mozilla.org/widget/clipboard;1";
          const kCBIID = Components.interfaces.nsIClipboard;
          this._CB = Components.classes[kCBContractID].getService(kCBIID);
        }
      return this._CB;
    },
    
  currentClipboard: null,
  /** 
   * Array/Object read (Object aFlavourList, long aClipboard, Bool aAnyFlag) ;
   *
   * returns the data in the clipboard
   * 
   * @param FlavourSet aFlavourSet
   *        formatted list of desired flavours
   * @param long aClipboard
   *        the clipboard to read data from (kSelectionClipboard/kGlobalClipboard)
   * @param Bool aAnyFlag
   *        should be false.
   **/
  read: function (aFlavourList, aClipboard, aAnyFlag)
    {
      this.currentClipboard = aClipboard;
      var data = nsTransferable.get(aFlavourList, this.getClipboardTransferable, aAnyFlag);
      return data.first.first;  // only support one item
    },
    
  /**
   * nsISupportsArray getClipboardTransferable (Object aFlavourList) ;
   * 
   * returns a nsISupportsArray of the item on the clipboard
   *
   * @param Object aFlavourList
   *        formatted list of desired flavours.
   **/
  getClipboardTransferable: function (aFlavourList)
    {
      const supportsContractID = "@mozilla.org/supports-array;1";
      const supportsIID = Components.interfaces.nsISupportsArray;
      var supportsArray = Components.classes[supportsContractID].createInstance(supportsIID);
      var trans = nsTransferable.createTransferable();
      for (var flavour in aFlavourList) 
        trans.addDataFlavor(flavour);
      nsClipboard.mClipboard.getData(trans, nsClipboard.currentClipboard)
      supportsArray.AppendElement(trans);
      return supportsArray;
    }
};

