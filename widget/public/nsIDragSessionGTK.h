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
*/

#ifndef nsIDragSessionGTK_h_
#define nsIDragSessionGTK_h_

#include "nsISupports.h"

#include <gtk/gtk.h>

typedef void (*nsIDragSessionGTKTimeCB)(guint32 *aTime);

#define NS_IDRAGSESSIONGTK_IID \
{ 0xa6b49c42, 0x1dd1, 0x11b2, { 0xb2, 0xdf, 0xc1, 0xd6, 0x1d, 0x67, 0x45, 0xcf } };

class nsIDragSessionGTK : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONGTK_IID)

  // Thse are all target methods - that is when the mozilla is a
  // target of a drag.  Any methods related to when mozilla starts a
  // drag are elsewhere.
    
  // this sets the last drag context used where mozilla is the target
  // of a drag
  NS_IMETHOD TargetSetLastContext  (GtkWidget      *aWidget,
                                    GdkDragContext *aContext,
                                    guint           aTime) = 0;
  // this is called at the beginning of a drag motion event
  NS_IMETHOD TargetStartDragMotion (void) = 0;
  // this is called at the end of a drag motion event
  NS_IMETHOD TargetEndDragMotion   (GtkWidget      *aWidget,
                                    GdkDragContext *aContext,
                                    guint           aTime) = 0;
  // this is called when data is received after being sent above
  NS_IMETHOD TargetDataReceived    (GtkWidget         *aWidget,
                                    GdkDragContext    *aContext,
                                    gint               aX,
                                    gint               aY,
                                    GtkSelectionData  *aSelection_data,
                                    guint              aInfo,
                                    guint32            aTime) = 0;
  // this sets a callback for time related fun
  NS_IMETHOD TargetSetTimeCallback (nsIDragSessionGTKTimeCB aCallback) = 0;

};

#endif /* nsIDragSessionGTK_h_ */
