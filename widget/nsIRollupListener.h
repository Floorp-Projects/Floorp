/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  /**
   * Notify the RollupListener that the widget did a Move or Resize.
   */
  virtual void NotifyGeometryChange() = 0;
};

#endif /* __nsIRollupListener_h__ */
