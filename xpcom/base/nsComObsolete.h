/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsComObsolete_h__
#define nsComObsolete_h__

/* These _IMPL_NS_*  defines should move into their own directories.  */
#ifdef _IMPL_NS_BASE
#define NS_BASE NS_EXPORT
#else
#define NS_BASE NS_IMPORT
#endif

#ifdef _IMPL_NS_NET
#define NS_NET NS_EXPORT
#else
#define NS_NET NS_IMPORT
#endif

#ifdef _IMPL_NS_DOM
#define NS_DOM NS_EXPORT
#else
#define NS_DOM NS_IMPORT
#endif

#ifdef _IMPL_NS_WIDGET
#define NS_WIDGET NS_EXPORT
#else
#define NS_WIDGET NS_IMPORT
#endif

#ifdef _IMPL_NS_VIEW
#define NS_VIEW NS_EXPORT
#else
#define NS_VIEW NS_IMPORT
#endif

#ifdef _IMPL_NS_GFXNONXP
#define NS_GFXNONXP NS_EXPORT
#define NS_GFXNONXP_(type) NS_EXPORT_(type)
#else
#define NS_GFXNONXP NS_IMPORT
#define NS_GFXNONXP_(type) NS_IMPORT_(type)
#endif

#ifdef _IMPL_NS_GFX
#define NS_GFX NS_EXPORT
#define NS_GFX_(type) NS_EXPORT_(type)
#else
#define NS_GFX NS_IMPORT
#define NS_GFX_(type) NS_IMPORT_(type)
#endif

#ifdef _IMPL_NS_PLUGIN
#define NS_PLUGIN NS_EXPORT
#else
#define NS_PLUGIN NS_IMPORT
#endif

#ifdef _IMPL_NS_APPSHELL
#define NS_APPSHELL NS_EXPORT
#else
#define NS_APPSHELL NS_IMPORT
#endif


/*
 * People who create their own Win32 MSDev projects to compile against mozilla
 * code *often* neglect to define XP_WIN and XP_WIN32. Rather than force
 * those definitions here - and risk having some code get compiled incorrectly
 * before this code is reached - we #error here to let the programmers know
 * that they must modify their build projects.
 * We would *like* to reduce the usage of these roughly synonymous defines.
 * But it is a big modular project with a lot of brainprint issues...
 * See bug: http://bugzilla.mozilla.org/show_bug.cgi?id=65727
 */
#if defined(_WIN32) && (!defined(XP_WIN) || !defined(XP_WIN32))
#error Add defines for XP_WIN and XP_WIN32 to your Win32 build project.
#endif


/* Define brackets for protecting C code from C++ */
#ifdef __cplusplus
#define NS_BEGIN_EXTERN_C extern "C" {
#define NS_END_EXTERN_C }
#else
#define NS_BEGIN_EXTERN_C
#define NS_END_EXTERN_C
#endif



#ifdef __cplusplus
#include "nsDebug.h"
#endif

#endif
