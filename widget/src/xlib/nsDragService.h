/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) 1998 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s):
 * Christopher Blizzard <blizzard@mozilla.org>
 * Peter Hartshorn <peter@igelaus.com.au>
*/

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include "nsIDragService.h"
#include "nsIDragSessionXlib.h"
#include "nsGUIEvent.h"
#include "nsIWidget.h"
#include "nsWidget.h"

/**
 * xlib shell based on gtk
 */

// Our drag pixmap
static char * drag_xpm[] = {
"32 32 5 1",
"       c none",
".      c #000000",
"+      c #1142d6",
"@      c #ffffff",
"#      c #878787",
"                                ",
"                                ",
"                                ",
"                                ",
"        +                       ",
"        ++                      ",
"        +++                     ",
"        ++++ .............      ",
"    +++++++++@@@@@@@@@@@@..     ",
"     +++++++++@@@@@@@@@@@.@.    ",
"      +++++++++@@@@@@@@@@.@@.   ",
"       +++++++++@@@@@@@@@.@@@.  ",
"        +++++++++@@@@@@@@.@@@@. ",
"         +++++++++@@@@@@@.......",
"        .#+++++++++@@@@@@@@@@@@.",
"        .##+++++++++@@@@@@@@@@@.",
"        .@##+++++++++@@@@@@@@@@.",
"        .@@##++++++++@@@@@@@@@@.",
"        .@@@##+++++++#@@@@@@@@@.",
"        .@@@@##++++++#@@@@@@@@@.",
"        .@@@@@##+++++#@@@@@@@@@.",
"        .@@@@@@#######@@@@@@@@@.",
"        .@@@@@@@######@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        .@@@@@@@@@@@@@@@@@@@@@@.",
"        ........................"};

class nsDragService : public nsBaseDragService, public nsIDragSessionXlib
{

public:

  NS_DECL_ISUPPORTS_INHERITED

  nsDragService();
  virtual ~nsDragService();

  // nsIDragService
  NS_IMETHOD InvokeDragSession (nsIDOMNode *aDOMNode,
				nsISupportsArray * anArrayTransferables,
                                nsIScriptableRegion * aRegion,
                                PRUint32 aActionType);
  NS_IMETHOD StartDragSession();
  NS_IMETHOD EndDragSession();

  // nsIDragSession
  NS_IMETHOD GetCurrentSession     (nsIDragSession **aSession);
  NS_IMETHOD SetCanDrop            (PRBool           aCanDrop);
  NS_IMETHOD GetCanDrop            (PRBool          *aCanDrop);
  NS_IMETHOD GetNumDropItems       (PRUint32 * aNumItems);
  NS_IMETHOD GetData               (nsITransferable * aTransferable, PRUint32 anItemIndex);
  NS_IMETHOD IsDataFlavorSupported (const char *aDataFlavor, PRBool *_retval);

  //nsIDragSessionXlib
  NS_IMETHOD IsDragging(PRBool *result);
  NS_IMETHOD UpdatePosition(PRInt32 x, PRInt32 y);

protected:
  static nsEventStatus PR_CALLBACK Callback(nsGUIEvent *event);

private:
  static nsWidget *sWidget;
  static Window sWindow;
  static Display *sDisplay;
  static PRBool mDragging;
  PRBool mCanDrop;

  nsWidget *mDropWidget;
  nsISupportsArray *mDataItems;
};

#endif // nsDragService_h__
