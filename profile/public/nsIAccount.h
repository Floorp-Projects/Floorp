/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
//#ifndef __AFXWIN_H__
//#error include 'stdafx.h' before including this file for PCH
//#endif
#ifndef nsIAccount_h__
#define nsIAccount_h__

#include "nsISupports.h"
#include "nsFileSpec.h"
#include "windows.h"
#include "winbase.h"

/** new ids **/
#define NS_IAccount_IID                                 \
  {/*{19EA6EB1-3D7A-11d3-B205-006008A6BBCE} */			\
	0x19ea6eb1,											\
	0x3d7a,												\
	0x11d3,												\
	{ 0xb2, 0x05, 0x00, 0x60, 0x08, 0xa6, 0xbb, 0xce }	\
  }	

#define NS_Account_CID                                  \
  { /*{19EA6EB2-3D7A-11d3-B205-006008A6BBCE}  */		\
	0x19ea6eb2,											\
	0x3d7a,												\
	0x11d3,												\
	{ 0xb2, 0x05, 0x00, 0x60, 0x08, 0xa6, 0xbb, 0xce }	\
  }	

#define NS_USING_Account 1

/*
 * Return values
 */

class nsIAccount: public nsISupports {
public:

  static const nsIID& GetIID(void) { static nsIID iid = NS_IAccount_IID; return iid; }

	// Initialize/shutdown
	NS_IMETHOD Startup(void) = 0;
	NS_IMETHOD Shutdown() = 0;

	// Getters
	NS_IMETHOD GetAcctConfig(nsString& AccountList) = 0;
	NS_IMETHOD GetModemConfig(nsString& ModemList) =0;
	NS_IMETHOD GetLocation(nsString& Locat) =0;
	NS_IMETHOD GetSiteName(nsString& SiteList) =0;
	NS_IMETHOD GetPhone(nsString& PhoneList) =0;
	NS_IMETHOD LoadValues(void)=0;
	NS_IMETHOD CheckForDun(nsString& dun)=0;

//    NS_IMETHOD GetModemConfig(nsString returnData)=0;
    NS_IMETHOD SetDialerConfig(char* returnData)=0;


	//	NS_IMETHOD PEPluginFunc( long selectorCode, void* paramBlock, void* returnData )=0;

#ifdef XP_PC
	OSVERSIONINFO *lpOsVersionInfo;    
#endif

	// Setters
/**
private:  
#ifdef WIN32
	OSVERSIONINFO *lpOsVersionInfo;    
#endif
**/
};

#endif /* nsIAccount_h__ */
