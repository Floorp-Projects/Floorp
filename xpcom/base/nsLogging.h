/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

#ifndef nsLogging_h__
#define nsLogging_h__

#include "nsILoggingService.h"

#ifdef NS_ENABLE_LOGGING

#include "nsHashtable.h"
#include "nsCOMPtr.h"

class nsLoggingService : public nsILoggingService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILOGGINGSERVICE

    nsLoggingService();
    virtual ~nsLoggingService();

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    nsresult Init();

protected:
    nsSupportsHashtable         mLogs;
    PRUint32                    mDefaultControlFlags;
    nsCOMPtr<nsILogEventSink>   mDefaultSink;
};

////////////////////////////////////////////////////////////////////////////////

class nsTimingData {
public:
    nsTimingData()
        : mStartTime(0),
          mTotalTime(0),
          mTotalSquaredTime(0),
          mTimingSamples(0) {
    }
    PRIntervalTime              mStartTime;
    double                      mTotalTime;
    double                      mTotalSquaredTime;
    PRUint32                    mTimingSamples;
};

////////////////////////////////////////////////////////////////////////////////

class nsLog : public nsILog
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILOG

    nsLog();
    virtual ~nsLog();

    nsresult Init(const char* name, PRUint32 controlFlags, nsILogEventSink* sink);

protected:
    char*                       mName;
    PRUint32                    mControlFlags;
    PRUint32                    mIndentLevel;
    PRUintn                     mThreadTimingDataIndex;
    nsTimingData                mTimingData;
    nsCOMPtr<nsILogEventSink>   mSink;
};

////////////////////////////////////////////////////////////////////////////////

class nsStandardLogEventSink : public nsIStandardLogEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSILOGEVENTSINK
    NS_DECL_NSISTANDARDLOGEVENTSINK

    nsStandardLogEventSink();
    virtual ~nsStandardLogEventSink();

protected:
    char*       mName;
    FILE*       mOutput;
    PRUint32    mDebugLevel;
    PRBool      mBeginningOfLine;
    PRBool      mCloseFile;
};

#endif // NS_ENABLE_LOGGING

#endif // nsLogging_h__



