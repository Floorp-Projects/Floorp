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
#ifndef nsDocLoader_h___
#define nsDocLoader_h___

#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsString.h"

class nsIBrowserWindow;
class nsITimer;
class nsVoidArray;
class nsViewerApp;

/* 
  This class loads creates and loads URLs until finished.
  When the doc counter reaches zero, the DocLoader (optionally)
  posts an exit application message
*/

class nsDocLoader : public nsIStreamObserver
{
public:
  nsDocLoader(nsIBrowserWindow* aBrowser, nsViewerApp* aViewer, 
              PRInt32 aSeconds, PRBool aPostExit,
              PRBool aJiggleLayout);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStreamObserver
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg);
    
  // Add a URL to the doc loader
  void AddURL(const char* aURL);
  void AddURL(const nsString& aURL);

  // Get the timer and the url list
  // needed to be available for the call back methods
  nsVoidArray*  GetTimers() 
  { return mTimers; }
  nsVoidArray*  GetURLList()
  { return mURLList; }


  void CreateOneShot(PRUint32 aDelay);
  void CreateRepeat(PRUint32 aDelay);
  void CallTest();

  // Set the delay (by default, the timer is set to one second)
  void SetDelay(PRInt32 aSeconds);
    
  // If set to TRUE the loader will post an exit message on
  // exit
  void SetExitOnDone(PRBool aPostExit);

  // Start Loading the list of documents and executing the 
  // Do Action Method on the documents
  void StartTimedLoading();

  // Start Loading the list of documents and executing the 
  // Do Action Method on the documents
  void StartLoading();


  // By default this method loads a document from
  // the list of documents added to the loader
  // Override in subclasses to get different behavior
  virtual void DoAction(PRInt32 aDocNum);

protected:
  virtual ~nsDocLoader();

  void CancelAll();

  void LoadDoc(PRInt32 aDocNum, PRBool aObserveIt);

  PRInt32           mDocNum;
  PRBool            mStart;
  PRBool            mJiggleLayout;
  PRInt32           mDelay;
  PRBool            mPostExit;
  nsIBrowserWindow* mBrowser;
  nsViewerApp*      mViewer;
  nsString          mURL;
  nsVoidArray*      mURLList;
  nsVoidArray*      mTimers;
};

#endif
