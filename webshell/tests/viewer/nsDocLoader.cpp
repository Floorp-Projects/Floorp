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

#include "nsDocLoader.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsVoidArray.h"
#include "plstr.h"
#include "nsIWebWidget.h"
#include "resources.h"
#include "nsString.h"

#include <windows.h>

/* 
  This class loads creates and loads URLs until finished.
  When the doc counter reaches zero, the DocLoader (optionally)
  posts an exit application message

*/

nsDocLoader::nsDocLoader(nsIWebWidget* aWebWidget, PRInt32 aSeconds, PRBool aPostExit)
{
  mStart = PR_FALSE;
  mDelay = aSeconds;
  mPostExit = aPostExit;
  mDocNum = 0;
  mWebWidget = aWebWidget;

  mTimers = new nsVoidArray();
  mURLList = new nsVoidArray();
}

nsDocLoader::~nsDocLoader()
{
  if (mURLList)
  { 
    PRInt32 count = mURLList->Count();
    for (int i = 0; i < count; i++)
    {
      nsString* s = (nsString*)mURLList->ElementAt(i);
      delete s;
    }
    delete mURLList;
    mURLList = nsnull;
  }

  if (mTimers)
  {
    delete mTimers;
    mTimers = nsnull;
  }

}
  
// Set the delay (by default, the timer is set to one second)
void nsDocLoader::SetDelay(PRInt32 aSeconds)
{
  mDelay = aSeconds;
}

    
// Add a URL to the doc loader
void nsDocLoader::AddURL(char* aURL)
{
  if (aURL)
  {
    if (mStart == PR_FALSE)
    {
      nsString* url;

      if (mURLList == nsnull)
        mURLList = new nsVoidArray();
        
      url = new nsString(aURL);
      mURLList->AppendElement((void*)url);
    }
  }
}

// Add a URL to the doc loader
void nsDocLoader::AddURL(nsString* aURL)
{
  if (aURL)
  {
    if (mStart == PR_FALSE)
    {
      nsString* url;

      if (mURLList == nsnull)
        mURLList = new nsVoidArray();
      url = new nsString(*aURL);
      mURLList->AppendElement((void*)url);
    }
  }
}

void nsDocLoader::StartTimedLoading()
{
  // Only start once!
  if (mStart == PR_FALSE)
  {
    mStart = PR_TRUE;
    if (mURLList)
    {
      CreateRepeat(mDelay*1000);    
    }
  }
}




/*
*
* BEGIN PURIFY METHODS
*
*/

nsDocLoader*  gDocLoader = nsnull;
void MyCallback (nsITimer *aTimer, void *aClosure)
{

  if (gDocLoader != nsnull)
  {
    nsVoidArray*  timers = gDocLoader->GetTimers();
    printf("Timer executed with delay %d\n", (int)aClosure);
    if (timers->RemoveElement(aTimer) == PR_TRUE) 
    {
      NS_RELEASE(aTimer);
      gDocLoader->CallTest();
    }
  }
}


void nsDocLoader::CreateOneShot(PRUint32 aDelay)
{
  nsITimer *timer;
  gDocLoader = this;
  NS_NewTimer(&timer);
  timer->Init(MyCallback, (void *)aDelay, aDelay);
  mTimers->AppendElement(timer);
}


void MyRepeatCallback (nsITimer *aTimer, void *aClosure)
{
  if (gDocLoader != nsnull)
  {
    nsVoidArray*  timers = gDocLoader->GetTimers();

    printf("Timer executed with delay %d\n", (int)aClosure);  
    if (timers->RemoveElement(aTimer) == PR_TRUE) 
    {
      NS_RELEASE(aTimer);
      gDocLoader->CallTest();
    }
    gDocLoader->CreateRepeat((PRUint32)aClosure);
  }
}

void nsDocLoader::CreateRepeat(PRUint32 aDelay)
{
  if (mTimers)
  {
    gDocLoader = this;
    nsITimer *timer;

    NS_NewTimer(&timer);
    timer->Init(MyRepeatCallback, (void *)aDelay, aDelay);

    mTimers->AppendElement(timer);
  }
}


void nsDocLoader::DoAction(PRInt32 aDocNum)
{
  if (aDocNum >= 0 && aDocNum < mURLList->Count())
  {
    nsString* url = (nsString*)mURLList->ElementAt(aDocNum);
    if (url)
      mWebWidget->LoadURL(*url);
  }
}


void nsDocLoader::CallTest()
{
  if (mDocNum < mURLList->Count())
  {
    DoAction(mDocNum++);
  }
  else
  {
    CancelAll();

    printf("Finished Running Tests \n");
    if (mPostExit)
    {
      printf("QUITTING APPLICATION \n");
      PostMessage(HWND_BROADCAST,(UINT)WM_COMMAND,(WPARAM)VIEWER_EXIT,0);
    }
  }
}




void nsDocLoader::CancelAll()
{
  int i, count = mTimers->Count();

  for (i=0; i < count; i++) 
  {
    nsITimer *timer = (nsITimer *)mTimers->ElementAt(i);

    if (timer != NULL) 
    {
      timer->Cancel();
      NS_RELEASE(timer);
    }
  }
  mTimers->Clear();
  delete mTimers;
  mTimers = nsnull;

  if (mURLList)
  {
    PRInt32 count = mURLList->Count();
    for (int i = 0; i < count; i++)
    {
      nsString* s = (nsString*)mURLList->ElementAt(i);
      delete s;
    }
    delete mURLList;
    mURLList = nsnull;
  }

}
