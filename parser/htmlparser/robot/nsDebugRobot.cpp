/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#include "nsIRobotSink.h"
#include "nsIRobotSinkObserver.h"
#include "nsIParser.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIURL.h"

static NS_DEFINE_IID(kIRobotSinkObserverIID, NS_IROBOTSINKOBSERVER_IID);

class RobotSinkObserver : public nsIRobotSinkObserver {
public:
  RobotSinkObserver() {
    NS_INIT_REFCNT();
  }

  ~RobotSinkObserver() {
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD ProcessLink(const nsString& aURLSpec);
};

NS_IMPL_ISUPPORTS(RobotSinkObserver, kIRobotSinkObserverIID);

NS_IMETHODIMP RobotSinkObserver::ProcessLink(const nsString& aURLSpec)
{
  fputs(aURLSpec, stdout);
  printf("\n");
  return NS_OK;
}

//----------------------------------------------------------------------

extern "C" NS_EXPORT int DebugRobot(nsVoidArray * workList)
{
  RobotSinkObserver* myObserver = new RobotSinkObserver();
  NS_ADDREF(myObserver);

  for (;;) {
    PRInt32 n = workList->Count();
    if (0 == n) {
      break;
    }
    nsString* urlName = (nsString*) workList->ElementAt(n - 1);
    workList->RemoveElementAt(n - 1);

    // Create url
    nsIURL* url;
    nsresult rv = NS_NewURL(&url, *urlName);
    if (NS_OK != rv) {
      printf("invalid URL: '");
      fputs(*urlName, stdout);
      printf("'\n");
      return -1;
    }
    delete urlName;

    nsIParser* parser;
    rv = NS_NewHTMLParser(&parser);
    if (NS_OK != rv) {
      printf("can't make parser\n");
      return -1;
    }

    nsIRobotSink* sink;
    rv = NS_NewRobotSink(&sink);
    if (NS_OK != rv) {
      printf("can't make parser\n");
      return -1;
    }
    sink->Init(url);
    sink->AddObserver(myObserver);

    parser->SetContentSink(sink);
    parser->Parse(url);
    NS_RELEASE(sink);
    NS_RELEASE(parser);
    NS_RELEASE(url);
  }

  NS_RELEASE(myObserver);

  return 0;
}
