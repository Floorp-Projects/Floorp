/*
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
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <nsString.h>
#include <nsIXRemoteWidgetHelper.h>

#ifndef __nsGtkMozRemoteHelper_h__
#define __nsGtkMozRemoteHelper_h__

class nsGtkMozRemoteHelper
{
public:
  nsGtkMozRemoteHelper();
  virtual ~nsGtkMozRemoteHelper();

  // interaction from the outside world
  static        void SetupVersion         (GdkWindow *aWindow);
  static    gboolean HandlePropertyChange (GtkWidget *aWidget,
                                           GdkEventProperty *aEvent,
                                           nsIWidget *ansIWidget);

 private:

  // internal methods
  static       void  EnsureAtoms     (void);

  static        Atom sMozVersionAtom;
  static        Atom sMozLockAtom;
  static        Atom sMozCommandAtom;
  static        Atom sMozResponseAtom;


};

// {84f94aac-1dd2-11b2-a05f-9b338fea662c}

#define NS_GTKXREMOTEWIDGETHELPER_CID \
  { 0x84f94aac, 0x1dd2, 0x11b2, \
  { 0xa0, 0x5f, 0x9b, 0x33, 0x8f, 0xea, 0x66, 0x2c } }

class nsGtkXRemoteWidgetHelper : public nsIXRemoteWidgetHelper {
 public:
  nsGtkXRemoteWidgetHelper();
  virtual ~nsGtkXRemoteWidgetHelper();

  NS_DECL_ISUPPORTS

  NS_IMETHOD EnableXRemoteCommands(nsIWidget *aWidget);
};

#endif /* __nsGtkMozRemoteHelper_h__ */
