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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsThrobber_h___
#define nsThrobber_h___

#include "nsweb.h"
#include "nsIImageObserver.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

class nsIImageGroup;
class nsITimer;
struct nsRect;

class nsThrobber : public nsIImageRequestObserver {
public:
  static nsThrobber* NewThrobber();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  // nsIImageRequestObserver
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

  // nsThrobber
  nsresult Init(nsIWidget* aParent, const nsRect& aBounds,
                const nsString& aFileNameMask, PRInt32 aNumImages);
  void Destroy();
  nsresult MoveTo(PRInt32 aX, PRInt32 aY);
  nsresult Show();
  nsresult Hide();
  nsresult Start();
  nsresult Stop();

protected:
  nsThrobber();
  virtual ~nsThrobber();

  void Tick();

  nsresult LoadThrobberImages(const nsString& aFileNameMask,
                              PRInt32 aNumImages);

  void DestroyThrobberImages();

  PRInt32        mWidth;
  PRInt32        mHeight;
  nsIWidget*     mWidget;
  nsVoidArray*   mImages;
  PRInt32        mNumImages;
  PRInt32        mIndex;
  nsIImageGroup* mImageGroup;
  nsITimer*      mTimer;
  PRBool         mRunning;
  PRUint32       mCompletedImages;

  PRInt32        mPreferredWidth;
  PRInt32        mPreferredHeight;

  static nsThrobber* FindThrobberFor(nsIWidget* aWidget);
  static void AddThrobber(nsThrobber* aThrobber);
  static void RemoveThrobber(nsThrobber* aThrobber);
  static nsEventStatus PR_CALLBACK HandleThrobberEvent(nsGUIEvent *aEvent);
  static void ThrobTimerCallback(nsITimer *aTimer, void *aClosure);

  static nsVoidArray* gThrobbers;
  static PRInt32 gNumThrobbers;
};

#endif /* nsThrobber_h___ */
