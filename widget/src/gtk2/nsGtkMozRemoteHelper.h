/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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
  static        void SetupVersion         (GdkWindow *aWindow,
                                           const char *aProfile,
                                           const char *aProgram);
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
  static        Atom sMozUserAtom;
  static        Atom sMozProfileAtom;
  static        Atom sMozProgramAtom;

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

  NS_IMETHOD EnableXRemoteCommands(nsIWidget *aWidget,
                                   const char *aProfile,
                                   const char *aProgram);
};

#endif /* __nsGtkMozRemoteHelper_h__ */
