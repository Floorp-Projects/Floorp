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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

#ifndef GTKTOOLKIT_H      
#define GTKTOOLKIT_H

#include "nsIToolkit.h"
#include "nsString.h"
#include <gtk/gtk.h>

/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsGTKToolkit : public nsIToolkit
{
public:
    nsGTKToolkit();
    virtual ~nsGTKToolkit();

    NS_DECL_ISUPPORTS

    NS_IMETHOD    Init(PRThread *aThread);

    void          CreateSharedGC(void);
    GdkGC         *GetSharedGC(void);
    
    /**
     * Get/set our value of DESKTOP_STARTUP_ID. When non-empty, this is applied
     * to the next toplevel window to be shown or focused (and then immediately
     * cleared).
     */ 
    void SetDesktopStartupID(const nsACString& aID) { mDesktopStartupID = aID; }
    void GetDesktopStartupID(nsACString* aID) { *aID = mDesktopStartupID; }

    /**
     * Get/set the timestamp value to be used, if non-zero, to focus the
     * next top-level window to be shown or focused (upon which it is cleared).
     */
    void SetFocusTimestamp(PRUint32 aTimestamp) { mFocusTimestamp = aTimestamp; }
    PRUint32 GetFocusTimestamp() { return mFocusTimestamp; }

private:
    GdkGC         *mSharedGC;
    nsCString      mDesktopStartupID;
    PRUint32       mFocusTimestamp;
};

#endif  // GTKTOOLKIT_H
