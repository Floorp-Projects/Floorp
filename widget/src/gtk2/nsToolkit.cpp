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

#include "nscore.h"  // needed for 'nsnull'
#include "nsGTKToolkit.h"
#include "nsWidgetAtoms.h"

//
// Static thread local storage index of the Toolkit 
// object associated with a given thread...
//
static PRUintn gToolkitTLSIndex = 0;

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsGTKToolkit::nsGTKToolkit()
{
    mSharedGC = nsnull;
    mFocusTimestamp = 0;
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsGTKToolkit::~nsGTKToolkit()
{
    if (mSharedGC) {
        g_object_unref(mSharedGC);
    }

    // Remove the TLS reference to the toolkit...
    PR_SetThreadPrivate(gToolkitTLSIndex, nsnull);
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsGTKToolkit, nsIToolkit)

void nsGTKToolkit::CreateSharedGC(void)
{
    GdkPixmap *pixmap;

    if (mSharedGC)
        return;

    pixmap = gdk_pixmap_new(NULL, 1, 1, gdk_rgb_get_visual()->depth);
    mSharedGC = gdk_gc_new(pixmap);
    g_object_unref(pixmap);
}

GdkGC *nsGTKToolkit::GetSharedGC(void)
{
    return (GdkGC *)g_object_ref(mSharedGC);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsGTKToolkit::Init(PRThread *aThread)
{
    CreateSharedGC();

    nsWidgetAtoms::RegisterAtoms();

    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Return the nsIToolkit for the current thread.  If a toolkit does not
// yet exist, then one will be created...
//
//-------------------------------------------------------------------------
NS_METHOD NS_GetCurrentToolkit(nsIToolkit* *aResult)
{
    nsIToolkit* toolkit = nsnull;
    nsresult rv = NS_OK;
    PRStatus status;

    // Create the TLS index the first time through...
    if (0 == gToolkitTLSIndex) {
        status = PR_NewThreadPrivateIndex(&gToolkitTLSIndex, NULL);
        if (PR_FAILURE == status) {
            rv = NS_ERROR_FAILURE;
        }
    }

    if (NS_SUCCEEDED(rv)) {
        toolkit = (nsIToolkit*)PR_GetThreadPrivate(gToolkitTLSIndex);

        //
        // Create a new toolkit for this thread...
        //
        if (!toolkit) {
            toolkit = new nsGTKToolkit();

            if (!toolkit) {
                rv = NS_ERROR_OUT_OF_MEMORY;
            } else {
                NS_ADDREF(toolkit);
                toolkit->Init(PR_GetCurrentThread());
                //
                // The reference stored in the TLS is weak.  It is
                // removed in the nsToolkit destructor...
                //
                PR_SetThreadPrivate(gToolkitTLSIndex, (void*)toolkit);
            }
        } else {
            NS_ADDREF(toolkit);
        }
        *aResult = toolkit;
    }

    return rv;
}


