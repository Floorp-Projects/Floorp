/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

