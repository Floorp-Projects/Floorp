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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "prlog.h"

#include "nsString.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gImgLog;

class LogScope {
public:
  LogScope(PRLogModuleInfo *aLog, void *from, const nsAReadableCString &fn) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s {ENTER}\n",
                                   mFrom, mFunc.get()));
  }

  /* const char * constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsAReadableCString &fn,
           const nsLiteralCString &paramName, const char *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s (%s=\"%s\") {ENTER}\n",
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* void ptr constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsAReadableCString &fn,
           const nsLiteralCString &paramName, const void *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s (%s=%p) {ENTER}\n",
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* PRInt32 constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsAReadableCString &fn,
           const nsLiteralCString &paramName, PRInt32 paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* PRUint32 constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsAReadableCString &fn,
           const nsLiteralCString &paramName, PRUint32 paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }


  ~LogScope() {
    PR_LOG(mLog, PR_LOG_DEBUG, ("[this=%p] %s {EXIT}\n",
                                   mFrom, mFunc.get()));
  }

private:
  PRLogModuleInfo *mLog;
  void *mFrom;
  nsCAutoString mFunc;
};


#define LOG_SCOPE(l, s) \
  LogScope LOG_SCOPE_TMP_VAR ##__LINE__ (l,                            \
                                         NS_STATIC_CAST(void *, this), \
                                         NS_LITERAL_CSTRING(s))

#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv) \
  LogScope LOG_SCOPE_TMP_VAR ##__LINE__ (l,                            \
                                         NS_STATIC_CAST(void *, this), \
                                         NS_LITERAL_CSTRING(s),        \
                                         NS_LITERAL_CSTRING(pn), pv)

#else
#define LOG_SCOPE(l, s)
#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv)
#endif
