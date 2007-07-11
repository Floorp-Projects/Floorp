/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"
#include "prinrval.h"

#include "nsString.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gImgLog;

#define GIVE_ME_MS_NOW() PR_IntervalToMilliseconds(PR_IntervalNow())

class LogScope {
public:
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get()));
  }

  /* const char * constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, const char *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* void ptr constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, const void *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=%p) {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* PRInt32 constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, PRInt32 paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* PRUint32 constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, PRUint32 paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }


  ~LogScope() {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {EXIT}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get()));
  }

private:
  PRLogModuleInfo *mLog;
  void *mFrom;
  nsCAutoString mFunc;
};


class LogFunc {
public:
  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get()));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, const char *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, const void *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%p\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }


  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, PRUint32 paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }

};


class LogMessage {
public:
  LogMessage(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
             const nsDependentCString &msg)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s -- %s\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                msg.get()));
  }
};

#define LOG_SCOPE(l, s) \
  LogScope LOG_SCOPE_TMP_VAR ##__LINE__ (l,                            \
                                         static_cast<void *>(this),    \
                                         NS_LITERAL_CSTRING(s))

#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv) \
  LogScope LOG_SCOPE_TMP_VAR ##__LINE__ (l,                            \
                                         static_cast<void *>(this),    \
                                         NS_LITERAL_CSTRING(s),        \
                                         NS_LITERAL_CSTRING(pn), pv)

#define LOG_FUNC(l, s)                  \
  LogFunc(l,                            \
          static_cast<void *>(this),    \
          NS_LITERAL_CSTRING(s))

#define LOG_FUNC_WITH_PARAM(l, s, pn, pv) \
  LogFunc(l,                              \
          static_cast<void *>(this),      \
          NS_LITERAL_CSTRING(s),          \
          NS_LITERAL_CSTRING(pn), pv)

#define LOG_STATIC_FUNC(l, s)           \
  LogFunc(l,                            \
          nsnull,                       \
          NS_LITERAL_CSTRING(s))

#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv) \
  LogFunc(l,                             \
          nsnull,                        \
          NS_LITERAL_CSTRING(s),         \
          NS_LITERAL_CSTRING(pn), pv)



#define LOG_MSG(l, s, m)                   \
  LogMessage(l,                            \
             static_cast<void *>(this),    \
             NS_LITERAL_CSTRING(s),        \
             NS_LITERAL_CSTRING(m))


#else
#define LOG_SCOPE(l, s)
#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv)
#define LOG_FUNC(l, s)
#define LOG_FUNC_WITH_PARAM(l, s, pn, pv)
#define LOG_STATIC_FUNC(l, s)
#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv)
#define LOG_MSG(l, s, m)
#endif

#define LOG_MSG_WITH_PARAM LOG_FUNC_WITH_PARAM
