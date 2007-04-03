/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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

#ifndef __nsPluginDirServiceProvider_h__
#define __nsPluginDirServiceProvider_h__

#include "nsIDirectoryService.h"

#if defined (XP_WIN)
#include "nsCOMArray.h"
#include <windows.h>
#endif

class nsISimpleEnumerator;

// Note: Our directory service provider scan keys are prefs which are check
//       for minimum versions compatibility
#define NS_WIN_JRE_SCAN_KEY            "plugin.scan.SunJRE"
#define NS_WIN_ACROBAT_SCAN_KEY        "plugin.scan.Acrobat"
#define NS_WIN_QUICKTIME_SCAN_KEY      "plugin.scan.Quicktime"
#define NS_WIN_WMP_SCAN_KEY            "plugin.scan.WindowsMediaPlayer"
#define NS_WIN_4DOTX_SCAN_KEY          "plugin.scan.4xPluginFolder"

//*****************************************************************************
// class nsPluginDirServiceProvider
//*****************************************************************************   

class nsPluginDirServiceProvider : public nsIDirectoryServiceProvider
{
public:
   nsPluginDirServiceProvider();
   
   NS_DECL_ISUPPORTS
   NS_DECL_NSIDIRECTORYSERVICEPROVIDER

#ifdef XP_WIN
   static nsresult GetPLIDDirectories(nsISimpleEnumerator **aEnumerator);
private:
   static nsresult GetPLIDDirectoriesWithHKEY(HKEY aKey, 
     nsCOMArray<nsILocalFile> &aDirs);
#endif

protected:
   virtual ~nsPluginDirServiceProvider();
};

#endif // __nsPluginDirServiceProvider_h__
