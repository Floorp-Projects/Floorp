/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifndef nsIPrefsManager_h__
#define nsIPrefsManager_h__

#include "nsISupports.h"

typedef int (*nsPrefChangedFunc) (const char *, void *); 

/*
 * Return values
 */

#define NS_PREF_VALUE_CHANGED 1

class nsIPrefsManager: public nsISupports {
public:
  // Getters
  NS_IMETHOD GetCharPref(const char *pref, 
			 char * return_buf, int * buf_length) = 0;
  NS_IMETHOD GetIntPref(const char *pref, int32 * return_int) = 0;	
  NS_IMETHOD GetBoolPref(const char *pref, PRBool *return_val) = 0;	
  NS_IMETHOD GetBinaryPref(const char *pref, 
			 void * return_val, int * buf_length) = 0;	
  NS_IMETHOD GetColorPref(const char *pref_name,
			uint8 *red, uint8 *green, uint8 *blue) = 0;
  NS_IMETHOD GetColorPrefDWord(const char *pref_name, uint32 *colorref) = 0;
  NS_IMETHOD GetRectPref(const char *pref_name, 
			 int16 *left, int16 *top, 
			 int16 *right, int16 *bottom) = 0;

  // Setters
  NS_IMETHOD SetCharPref(const char *pref,const char* value) = 0;
  NS_IMETHOD SetIntPref(const char *pref,int32 value) = 0;
  NS_IMETHOD SetBoolPref(const char *pref,PRBool value) = 0;
  NS_IMETHOD SetBinaryPref(const char *pref,void * value, long size) = 0;
  NS_IMETHOD SetColorPref(const char *pref_name, 
			  uint8 red, uint8 green, uint8 blue) = 0;
  NS_IMETHOD SetColorPrefDWord(const char *pref_name, uint32 colorref) = 0;
  NS_IMETHOD SetRectPref(const char *pref_name, 
			 int16 left, int16 top, int16 right, int16 bottom) = 0;

  // Get Defaults
  NS_IMETHOD GetDefaultCharPref(const char *pref, 
				char * return_buf, int * buf_length) = 0;
  NS_IMETHOD GetDefaultIntPref(const char *pref, int32 * return_int) = 0;
  NS_IMETHOD GetDefaultBoolPref(const char *pref, PRBool *return_val) = 0;
  NS_IMETHOD GetDefaultBinaryPref(const char *pref, 
				  void * return_val, int * buf_length) = 0;
  NS_IMETHOD GetDefaultColorPref(const char *pref_name, 
				 uint8 *red, uint8 *green, uint8 *blue) = 0;
  NS_IMETHOD GetDefaultColorPrefDWord(const char *pref_name, 
				      uint32 *colorref) = 0;
  NS_IMETHOD GetDefaultRectPref(const char *pref_name, 
				int16 *left, int16 *top, 
				int16 *right, int16 *bottom) = 0;

  // Set defaults
  NS_IMETHOD SetDefaultCharPref(const char *pref,const char* value) = 0;
  NS_IMETHOD SetDefaultIntPref(const char *pref,int32 value) = 0;
  NS_IMETHOD SetDefaultBoolPref(const char *pref,PRBool value) = 0;
  NS_IMETHOD SetDefaultBinaryPref(const char *pref,
				  void * value, long size) = 0;
  NS_IMETHOD SetDefaultColorPref(const char *pref_name, 
				 uint8 red, uint8 green, uint8 blue) = 0;
  NS_IMETHOD SetDefaultRectPref(const char *pref_name, 
				int16 left, int16 top, 
				int16 right, int16 bottom) = 0;
  
  // Pref info
  NS_IMETHOD PrefIsLocked(const char *pref_name, PRBool *res) = 0;

  // Callbacks
  NS_IMETHOD RegisterCallback( const char* domain,
			       nsPrefChangedFunc callback, 
			       void* instance_data ) = 0;
  NS_IMETHOD UnregisterCallback( const char* domain,
				 nsPrefChangedFunc callback, 
				 void* instance_data ) = 0;
};

#define NS_IPREFSMANAGER_IID                         \
{ /* e10fb550-62cb-11d2-8164-006008119d7a */         \
    0xe10fb550,                                      \
    0x62cb,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#define NS_PREFSMANAGER_CID                          \
{ /* fa8fa650-62cb-11d2-8164-006008119d7a */         \
    0xfa8fa650,                                      \
    0x62cb,                                          \
    0x11d2,                                          \
    {0x81, 0x64, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif /* nsIPrefsManager_h__ */
