/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Beard
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

#ifndef nsIScrollPositionListener_h___
#define nsIScrollPositionListener_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsTArray.h"
#include "nsIWidget.h"

// forward declarations
class nsIScrollableView;

// IID for the nsIScrollPositionListener interface
#define NS_ISCROLLPOSITIONLISTENER_IID \
  { 0x9654a477, 0x49a7, 0x4aea, \
    { 0xb7, 0xe3, 0x90, 0xe5, 0xe5, 0xd4, 0x28, 0xcd } }

/**
 * Provides a way for a client of an nsIScrollableView to learn about scroll position
 * changes.
 */
class nsIScrollPositionListener : public nsISupports {
public:
	NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCROLLPOSITIONLISTENER_IID)

	NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY) = 0;
	// The scrollframe implementation of this method appends a list of widget
	// configuration requests to aConfigurations. No other implementor
	// should touch it.
	virtual void ViewPositionDidChange(nsIScrollableView* aScrollable,
	                                   nsTArray<nsIWidget::Configuration>* aConfigurations) = 0;
	NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScrollPositionListener,
                              NS_ISCROLLPOSITIONLISTENER_IID)

#endif /* nsIScrollPositionListener_h___ */

