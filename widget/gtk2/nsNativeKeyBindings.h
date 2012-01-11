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

#ifndef nsNativeKeyBindings_h_
#define nsNativeKeyBindings_h_

// X.h defines KeyPress
#ifdef KeyPress
#undef KeyPress
#endif

#include "nsINativeKeyBindings.h"
#include <gtk/gtk.h>

enum NativeKeyBindingsType {
  eKeyBindings_Input,
  eKeyBindings_TextArea
};

#define NS_NATIVEKEYBINDINGSINPUT_CID \
{0x5c337258, 0xa580, 0x472e, {0x86, 0x15, 0xf2, 0x77, 0xdd, 0xc5, 0xbb, 0x06}}

#define NS_NATIVEKEYBINDINGSINPUT_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "input"

#define NS_NATIVEKEYBINDINGSTEXTAREA_CID \
{0x2a898043, 0x180f, 0x4c8b, {0x8e, 0x54, 0x41, 0x0c, 0x7a, 0x54, 0x0f, 0x27}}

#define NS_NATIVEKEYBINDINGSTEXTAREA_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "textarea"

#define NS_NATIVEKEYBINDINGSEDITOR_CID \
{0xf916ebfb, 0x78ef, 0x464b, {0x94, 0xd0, 0xa6, 0xf2, 0xca, 0x32, 0x00, 0xae}}

#define NS_NATIVEKEYBINDINGSEDITOR_CONTRACTID \
NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "editor"

class nsNativeKeyBindings : public nsINativeKeyBindings
{
public:
  NS_HIDDEN_(void) Init(NativeKeyBindingsType aType);

  NS_DECL_ISUPPORTS

  // nsINativeKeyBindings
  virtual NS_HIDDEN_(bool) KeyDown(const nsNativeKeyEvent& aEvent,
                                     DoCommandCallback aCallback,
                                     void *aCallbackData);

  virtual NS_HIDDEN_(bool) KeyPress(const nsNativeKeyEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void *aCallbackData);

  virtual NS_HIDDEN_(bool) KeyUp(const nsNativeKeyEvent& aEvent,
                                   DoCommandCallback aCallback,
                                   void *aCallbackData);

private:
  ~nsNativeKeyBindings() NS_HIDDEN;

  bool KeyPressInternal(const nsNativeKeyEvent& aEvent,
                          DoCommandCallback aCallback,
                          void *aCallbackData,
                          PRUint32 aKeyCode);

  GtkWidget *mNativeTarget;
};

#endif
