/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsPluginSafety_h__
#define nsPluginSafety_h__

#if defined(XP_PC) && !defined(XP_OS2)
#define CALL_SAFETY_ON
#endif

#ifdef CALL_SAFETY_ON

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, library) \
PR_BEGIN_MACRO                                     \
  nsresult res;                                    \
  PRBool dontdoit = PR_FALSE;                      \
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &res));\
  if(NS_SUCCEEDED(res) && (prefs != nsnull))       \
  {                                                \
    res = prefs->GetBoolPref("plugin.dont_try_safe_calls", &dontdoit);\
    if(NS_FAILED(res))                             \
      dontdoit = FALSE;                            \
  }                                                \
  if(dontdoit)                                     \
    ret = fun;                                     \
  else                                             \
  {                                                \
    try                                            \
    {                                              \
      ret = fun;                                   \
    }                                              \
    catch(...)                                     \
    {                                              \
      nsCOMPtr<nsIPluginHost> host(do_GetService(kCPluginManagerCID, &res));\
      if(NS_SUCCEEDED(res) && (host != nsnull))    \
        host->HandleBadPlugin(library);            \
      ret = (NPError)NS_ERROR_FAILURE;             \
    }                                              \
  }                                                \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, library) \
PR_BEGIN_MACRO                              \
  nsresult res;                             \
  PRBool dontdoit = PR_FALSE;               \
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &res));\
  if(NS_SUCCEEDED(res) && (prefs != nsnull))\
  {                                         \
    res = prefs->GetBoolPref("plugin.dont_try_safe_calls", &dontdoit);\
    if(NS_FAILED(res))                      \
      dontdoit = FALSE;                     \
  }                                         \
  if(dontdoit)                              \
    fun;                                    \
  else                                      \
  {                                         \
    try                                     \
    {                                       \
      fun;                                  \
    }                                       \
    catch(...)                              \
    {                                       \
      nsCOMPtr<nsIPluginHost> host(do_GetService(kCPluginManagerCID, &res));\
      if(NS_SUCCEEDED(res) && (host != nsnull))\
        host->HandleBadPlugin(library);     \
    }                                       \
  }                                         \
PR_END_MACRO

#else // vanilla calls

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, library) \
PR_BEGIN_MACRO                                     \
  ret = fun;                                       \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, library) \
PR_BEGIN_MACRO                              \
  fun;                                      \
PR_END_MACRO

#endif // CALL_SAFETY_ON

#endif //nsPluginSafety_h__

