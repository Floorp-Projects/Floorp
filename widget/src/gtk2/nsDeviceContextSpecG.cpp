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

#include <glib.h>
#include "nsDeviceContextSpecG.h"
//#include "prmem.h"
//#include "plstr.h"

// this isn't needed since we arn't useing getenv() anymore in here (pav)
//#include "stdlib.h"  // getenv() on Solaris/CC

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecGTK
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecGTK :: nsDeviceContextSpecGTK()
{
  NS_INIT_REFCNT();
	
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecGTK :: ~nsDeviceContextSpecGTK()
{


}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecGTK, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecGTK)
NS_IMPL_RELEASE(nsDeviceContextSpecGTK)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK :: Init(PRBool	aQuiet)
{
  gchar *path;

  // XXX for now, neutering this per rickg until dcone can play with it
  
  return NS_ERROR_FAILURE;

  // XXX these settings should eventually come out of preferences 

  mPrData.toPrinter = PR_TRUE;
  mPrData.fpf = PR_TRUE;
  mPrData.grayscale = PR_FALSE;
  mPrData.size = SizeLetter;
  mPrData.stream = (FILE *) NULL;
  sprintf( mPrData.command, "lpr" );

  // PWD, HOME, or fail 

  path = g_get_home_dir();
  sprintf(mPrData.path, "%s/netscape.ps", path);
  g_free(path);

  ::UnixPrDialog( &mPrData );
  if ( mPrData.cancel == PR_TRUE ) 
	return NS_ERROR_FAILURE;
  else
        return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecGTK :: ClosePrintManager()
{
	return NS_OK;
}  
