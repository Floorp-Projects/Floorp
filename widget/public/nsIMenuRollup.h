/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#ifndef nsIMenuRollup_h___
#define nsIMenuRollup_h___

#include "nsISupports.h"
#include "nsTArray.h"

class nsIWidget;
class nsIContent;

#define NS_IMENUROLLUP_IID \
  {0x61c9d01f, 0x8a4c, 0x4bb0, \
    { 0xa9, 0x90, 0xeb, 0xf6, 0x65, 0x4c, 0xda, 0x61 }}

class nsIMenuRollup : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IMENUROLLUP_IID)

  /*
   * Retrieve the widgets for open menus are store them in the array
   * aWidgetChain. The number of menus of the same type should be returned,
   * for example, if a context menu is open, return only the number of menus
   * that are part of the context menu chain. This allows closing up only
   * those menus in different situations.
   */
  virtual PRUint32 GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain) = 0;

  /**
   * Adjust the position of open panels when a window is moved or resized.
   */ 
  virtual void AdjustPopupsOnWindowChange(void) = 0;

};

  NS_DEFINE_STATIC_IID_ACCESSOR(nsIMenuRollup, NS_IMENUROLLUP_IID)

#endif
