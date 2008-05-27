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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#ifndef nsINativeKeyBindings_h_
#define nsINativeKeyBindings_h_

#include "nsISupports.h"
#include "nsEvent.h"

#define NS_INATIVEKEYBINDINGS_IID \
{0x606c54e7, 0x0593, 0x4750, {0x99, 0xd9, 0x4e, 0x1b, 0xcc, 0xec, 0x98, 0xd9}}

#define NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX \
  "@mozilla.org/widget/native-key-bindings;1?type="

struct nsNativeKeyEvent
{
  nsEvent *nativeEvent; // see bug 406407 to see how this is used
  PRUint32 keyCode;
  PRUint32 charCode;
  PRBool   altKey;
  PRBool   ctrlKey;
  PRBool   shiftKey;
  PRBool   metaKey;
};

class nsINativeKeyBindings : public nsISupports
{
 public:
  typedef void (*DoCommandCallback)(const char *, void*);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INATIVEKEYBINDINGS_IID)

  virtual NS_HIDDEN_(PRBool) KeyDown(const nsNativeKeyEvent& aEvent,
                                     DoCommandCallback aCallback,
                                     void *aCallbackData) = 0;

  virtual NS_HIDDEN_(PRBool) KeyPress(const nsNativeKeyEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void *aCallbackData) = 0;

  virtual NS_HIDDEN_(PRBool) KeyUp(const nsNativeKeyEvent& aEvent,
                                   DoCommandCallback aCallback,
                                   void *aCallbackData) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsINativeKeyBindings, NS_INATIVEKEYBINDINGS_IID)

#endif
