/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsPluginSafety_h__
#define nsPluginSafety_h__

#if defined(XP_PC) && !defined(XP_OS2)
#define CALL_SAFETY_ON
#endif

#ifdef CALL_SAFETY_ON

static PRBool gSkipPluginSafeCalls = PR_FALSE;

#define NS_INIT_PLUGIN_SAFE_CALLS                               \
PR_BEGIN_MACRO                                                  \
  nsresult res;                                                 \
  nsCOMPtr<nsIPref> pref(do_GetService(kPrefServiceCID, &res)); \
  if(NS_SUCCEEDED(res) && pref)                                 \
    res = pref->GetBoolPref("plugin.dont_try_safe_calls", &gSkipPluginSafeCalls);\
PR_END_MACRO

#define NS_TRY_SAFE_CALL_RETURN(ret, fun, library) \
PR_BEGIN_MACRO                                     \
  if(gSkipPluginSafeCalls)                         \
    ret = fun;                                     \
  else                                             \
  {                                                \
    try                                            \
    {                                              \
      ret = fun;                                   \
    }                                              \
    catch(...)                                     \
    {                                              \
      nsresult res;                                \
      nsCOMPtr<nsIPluginHost> host(do_GetService(kCPluginManagerCID, &res));\
      if(NS_SUCCEEDED(res) && (host != nsnull))    \
        host->HandleBadPlugin(library);            \
      ret = (NPError)NS_ERROR_FAILURE;             \
    }                                              \
  }                                                \
PR_END_MACRO

#define NS_TRY_SAFE_CALL_VOID(fun, library) \
PR_BEGIN_MACRO                              \
  if(gSkipPluginSafeCalls)                  \
    fun;                                    \
  else                                      \
  {                                         \
    try                                     \
    {                                       \
      fun;                                  \
    }                                       \
    catch(...)                              \
    {                                       \
      nsresult res;                         \
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

