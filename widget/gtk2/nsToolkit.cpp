/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"  // needed for 'nsnull'
#include "nsGTKToolkit.h"

nsGTKToolkit* nsGTKToolkit::gToolkit = nsnull;

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsGTKToolkit::nsGTKToolkit()
  : mSharedGC(nsnull), mFocusTimestamp(0)
{
    CreateSharedGC();
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
}

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

//-------------------------------------------------------------------------------
// Return the toolkit. If a toolkit does not yet exist, then one will be created.
//-------------------------------------------------------------------------------
// static
nsGTKToolkit* nsGTKToolkit::GetToolkit()
{
    if (!gToolkit) {
        gToolkit = new nsGTKToolkit();
    }
 
    return gToolkit;
}
