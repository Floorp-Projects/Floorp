/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Chris Saari <saari@netscape.com>
 */

#ifndef __imgContainer_h__
#define __imgContainer_h__

#include "imgIContainer.h"

#include "imgIContainerObserver.h"

#include "nsSize.h"

#include "nsSupportsArray.h"

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"

#define NS_IMGCONTAINER_CID \
{ /* 5e04ec5e-1dd2-11b2-8fda-c4db5fb666e0 */         \
     0x5e04ec5e,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8f, 0xda, 0xc4, 0xdb, 0x5f, 0xb6, 0x66, 0xe0} \
}

class imgContainer : public imgIContainer,
                     public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINER

  NS_IMETHOD_(void) Notify(nsITimer *timer);

  imgContainer();
  virtual ~imgContainer();

private:
  /* additional members */
  nsSupportsArray mFrames;
  nsSize mSize;
  PRUint32 mCurrentFrame;
  PRUint32 mCurrentAnimationFrame;
  PRBool   mCurrentFrameIsFinishedDecoding;
  PRBool   mDoneDecoding;
  
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<imgIContainerObserver> mObserver;
};

#endif /* __imgContainer_h__ */