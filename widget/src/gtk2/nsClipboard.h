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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __nsClipboard_h_
#define __nsClipboard_h_

#include "nsIClipboard.h"
#include "nsClipboardPrivacyHandler.h"
#include "nsAutoPtr.h"
#include <gtk/gtk.h>

class nsClipboard : public nsIClipboard
{
public:
    nsClipboard();
    virtual ~nsClipboard();
    
    NS_DECL_ISUPPORTS
    
    NS_DECL_NSICLIPBOARD

    // Make sure we are initialized, called from the factory
    // constructor
    nsresult  Init                (void);
    // Someone requested the selection from the hidden widget
    void      SelectionGetEvent   (GtkWidget         *aWidget,
                                   GtkSelectionData  *aSelectionData,
                                   guint              aTime);
    void      SelectionClearEvent (GtkWidget         *aWidget,
                                   GdkEventSelection *aEvent);


private:
    // Utility methods
    static GdkAtom               GetSelectionAtom (PRInt32 aWhichClipboard);
    static GtkSelectionData     *GetTargets       (GdkAtom aWhichClipboard);

    // Get our hands on the correct transferable, given a specific
    // clipboard
    nsITransferable             *GetTransferable  (PRInt32 aWhichClipboard);

    // Add a target type to the hidden widget
    void                         AddTarget        (GdkAtom aName,
                                                   GdkAtom aClipboard);

    // The hidden widget where we do all of our operations
    GtkWidget                   *mWidget;
    // Hang on to our owners and transferables so we can transfer data
    // when asked.
    nsCOMPtr<nsIClipboardOwner>  mSelectionOwner;
    nsCOMPtr<nsIClipboardOwner>  mGlobalOwner;
    nsCOMPtr<nsITransferable>    mSelectionTransferable;
    nsCOMPtr<nsITransferable>    mGlobalTransferable;
    nsRefPtr<nsClipboardPrivacyHandler> mPrivacyHandler;

};

#endif /* __nsClipboard_h_ */
