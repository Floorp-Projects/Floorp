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
#include "nsWindow.h"
#include "prmon.h"
#include "prtime.h"
#include "nsGUIEvent.h"
#include <Pt.h>
#include "nsPhWidgetLog.h"


NS_DEFINE_IID(kIToolkitIID,  NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()  
{
  NS_INIT_REFCNT();

  mWidget = NULL;
}


//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::~nsToolkit - Not Implemented.\n"));
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::Init this=<%p> aThread=<%p>\n", this, aThread));
  if( aThread )
  {
/*
    PtArg_t arg[5];
    PhPoint_t pos={-1000,-1000};
    PhDim_t dim={50,50};

    printf( "  Creating the hidden parent window.\n" );

    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetArg( &arg[2], Pt_ARG_WINDOW_RENDER_FLAGS, Ph_WM_RENDER_TITLE | Ph_WM_RENDER_CLOSE, 0xFFFFFFFF );
    mWidget = PtCreateWidget( PtWindow, NULL, 3, arg );
    if( mWidget )
    {
      PtRealizeWidget( mWidget );
      printf( "nsToolkit->mWidget = %p\n", mWidget );
    }
*/
  }
  else
  {
    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsToolkit::Init - ERROR: Wants to create a thread!\n"));
  }

  return NS_OK;
}


