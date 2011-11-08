/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rod Spears <rods@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __nsIRollupListener_h__
#define __nsIRollupListener_h__

#include "nsTArray.h"

class nsIContent;
class nsIWidget;

class nsIRollupListener {
 public: 

  /**
   * Notifies the object to rollup, optionally returning the node that
   * was just rolled up.
   *
   * aCount is the number of popups in a chain to close. If this is
   * PR_UINT32_MAX, then all popups are closed.
   * If aGetLastRolledUp is true, then return the last rolled up popup,
   * if this is supported.
   */
  virtual nsIContent* Rollup(PRUint32 aCount, bool aGetLastRolledUp = false) = 0;

  /**
   * Asks the RollupListener if it should rollup on mousevents
   */
  virtual bool ShouldRollupOnMouseWheelEvent() = 0;

  /**
   * Asks the RollupListener if it should rollup on mouse activate, eg. X-Mouse
   */
  virtual bool ShouldRollupOnMouseActivate() = 0;

  /*
   * Retrieve the widgets for open menus and store them in the array
   * aWidgetChain. The number of menus of the same type should be returned,
   * for example, if a context menu is open, return only the number of menus
   * that are part of the context menu chain. This allows closing up only
   * those menus in different situations. The returned value should be exactly
   * the same number of widgets added to aWidgetChain.
   */
  virtual PRUint32 GetSubmenuWidgetChain(nsTArray<nsIWidget*> *aWidgetChain) = 0;
};

#endif /* __nsIRollupListener_h__ */
