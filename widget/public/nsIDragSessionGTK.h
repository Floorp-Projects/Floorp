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

  NS_IMETHOD SetLastContext  (GtkWidget      *aWidget,
                              GdkDragContext *aContext,
                              guint           aTime) = 0;
  NS_IMETHOD StartDragMotion (GtkWidget      *aWidget,
                              GdkDragContext *aContext,
                              guint           aTime) = 0;
  NS_IMETHOD EndDragMotion   (GtkWidget      *aWidget,
                              GdkDragContext *aContext,
                              guint           aTime) = 0;
  NS_IMETHOD SetDataReceived (GtkWidget         *aWidget,
                              GdkDragContext    *context,
                              gint               x,
                              gint               y,
                              GtkSelectionData  *selection_data,
                              guint              info,
                              guint32            aTime) = 0;
  NS_IMETHOD DataGetSignal   (GtkWidget        *widget,
                              GdkDragContext   *context,
                              GtkSelectionData *selection_data,
                              guint             info,
                              guint32           aTime,
                              gpointer          data) = 0;
  NS_IMETHOD SetTimeCallback (nsIDragSessionGTKTimeCB aCallback) = 0;

};

#endif /* nsIDragSessionGTK_h_ */
