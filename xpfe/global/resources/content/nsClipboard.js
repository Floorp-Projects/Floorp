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
 * nsClipboard - wrapper around nsIClipboard and nsITransferable
 *               that simplifies access to the clipboard. 
 **/ 
var nsClipboard = {
  get mClipboard()
    {
      return nsJSComponentManager.getService("@mozilla.org/widget/clipboard;1",
                                             "nsIClipboard");
    },
    
  mCurrentClipboard: null,
  /** 
   * Array/Object read (Object aFlavourList, long aClipboard, Bool aAnyFlag) ;
   *
   * returns the data in the clipboard
   * 
   * @param Object aFlavourList
   *        formatted list of desired flavours
   * @param long aClipboard
   *        the clipboard to read data from (kSelectionClipboard/kGlobalClipboard)
   * @param Bool aAnyFlag
   *        should be false.
   **/
  read: function (aFlavourList, aClipboard, aAnyFlag)
    {
      this.mCurrentClipboard = aClipboard;
      var data = nsTransferable.get(aFlavourList, this.getClipboardTransferable, aAnyFlag);
      return data;
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
      var supportsArray = nsJSSupportsUtils.createSupportsArray();
      var trans = nsTransferable.createTransferable();
      for (var flavour in aFlavourList) 
        trans.addDataFlavor(flavour);
      nsClipboard.mClipboard.getData(trans, nsClipboard.mCurrentClipboard)
      supportsArray.AppendElement(trans);
      return supportsArray;
    }
};

