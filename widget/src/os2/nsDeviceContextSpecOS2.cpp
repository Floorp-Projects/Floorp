/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#include "nsGfxDefs.h"
#include "libprint.h"

#include "nsDeviceContextSpecOS2.h"

nsDeviceContextSpecOS2::nsDeviceContextSpecOS2()
{ 
   NS_INIT_REFCNT();
   mQueue = nsnull;
}

nsDeviceContextSpecOS2::~nsDeviceContextSpecOS2()
{
  if( mQueue)
     PrnClosePrinter( mQueue);
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecOS2, nsIDeviceContextSpec)

nsresult nsDeviceContextSpecOS2::Init( PRTQUEUE *pq)
{
   mQueue = pq;
   return NS_OK;
}

nsresult nsDeviceContextSpecOS2::GetPRTQUEUE( PRTQUEUE *&p)
{
   p = mQueue;
   return NS_OK;
}
