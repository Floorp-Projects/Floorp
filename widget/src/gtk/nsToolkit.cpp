/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "plevent.h"


//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
  NS_INIT_REFCNT();
  mSharedGC = nsnull;
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

void nsToolkit::CreateSharedGC(void)
{
  GdkPixmap *pixmap;

  if (mSharedGC)
    return;

  pixmap = ::gdk_pixmap_new (NULL, 1, 1, gdk_rgb_get_visual()->depth);
  mSharedGC = ::gdk_gc_new (pixmap);
  gdk_pixmap_unref (pixmap);
}

GdkGC *nsToolkit::GetSharedGC(void)
{
  mSharedGC = gdk_gc_ref(mSharedGC);
  return mSharedGC;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  CreateSharedGC();

  return NS_OK;
}
