/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIClipboardCommands_h___
#define nsIClipboardCommands_h___

#include "nsISupports.h"
#include "prtypes.h"

/* {b8100c90-73be-11d2-92a5-00105a1b0d64} */
#define NS_ICLIPBOARDCOMMANDS_IID \
{ 0xb8100c90, 0x73be, 0x11d2,     \
  {0x92, 0xa5, 0x00, 0x10, 0x5a, 0x1b, 0x0d, 0x64} }

/**
 */
class nsIClipboardCommands : public nsISupports
{
public:
  /**
   * Returns whether there is a selection and it is not read-only.
   */
  NS_IMETHOD CanCutSelection(PRBool* aResult) = 0;

  /**
   * Returns whether there is a selection and it is copyable.
   */
  NS_IMETHOD CanCopySelection(PRBool* aResult) = 0;

  /**
   * Returns whether the current contents of the clipboard can be
   * pasted and if the current selection is not read-only.
   */
  NS_IMETHOD CanPasteSelection(PRBool* aResult) = 0;

  /**
   * Cut the current selection onto the clipboard.
   */
  NS_IMETHOD CutSelection(void) = 0;

  /**
   * Copy the current selection onto the clipboard.
   */
  NS_IMETHOD CopySelection(void) = 0;

  /**
   * Paste the contents of the clipboard into the current selection.
   */
  NS_IMETHOD PasteSelection(void) = 0;

  /**
   * Select the entire contents.
   */
  NS_IMETHOD SelectAll(void) = 0;

  /**
   * Clear the current selection (if any).
   */
  NS_IMETHOD SelectNone(void) = 0;
};


#endif /* nsIClipboardCommands_h___ */
