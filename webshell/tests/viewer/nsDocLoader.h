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
#include "nsISupports.h"


class nsIWebWidget;
class nsITimer;
class nsVoidArray;
class nsString;

/* 
  This class loads creates and loads URLs until finished.
  When the doc counter reaches zero, the DocLoader (optionally)
  posts an exit application message
*/

class nsDocLoader
{
  public:
    nsDocLoader(nsIWebWidget* aWebWidget, PRInt32 aSeconds=1, PRBool aPostExit=PR_TRUE);
    virtual ~nsDocLoader();
    
    // Add a URL to the doc loader
    void AddURL(char* aURL);
    void AddURL(nsString* aURL);

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


    // By default this method loads a document from
    // the list of documents added to the loader
    // Override in subclasses to get different behavior
    virtual void DoAction(PRInt32 aDocNum);


  private:

    void CancelAll();



    PRInt32       mDocNum;
    PRBool        mStart;
    PRInt32       mDelay;
    PRBool        mPostExit;
    nsIWebWidget* mWebWidget;

    nsVoidArray*  mURLList;
    nsVoidArray*  mTimers;
};



#endif