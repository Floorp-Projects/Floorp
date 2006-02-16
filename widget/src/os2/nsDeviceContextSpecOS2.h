/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Fairhurst <john_fairhurst@iname.com>
 *   IBM Corp.
 */

#ifndef _nsDeviceContextSpecOS2_h
#define _nsDeviceContextSpecOS2_h

#include "nsGfxDefs.h"

#include "nsIDeviceContextSpec.h"
#include "libprint.h"

class nsDeviceContextSpecOS2 : public nsIDeviceContextSpec
{
public:
  nsDeviceContextSpecOS2();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init(PRTQUEUE *pq);
  NS_IMETHOD GetPRTQUEUE(PRTQUEUE *&p);

protected:
  virtual ~nsDeviceContextSpecOS2();

  PRTQUEUE *mQueue;
};

#endif
