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

#ifndef nsIPref_h__
#define nsIPref_h__

#include "jsapi.h"
#include "nsISupports.h"

class nsFileSpec;

/* Temporarily conditionally compile PrefChangedFunc typedef.
** During migration from old libpref to nsIPref we need it in
** both header files.  Eventually prefapi.h will become a private
** file.  The two types need to be in sync for now.  Certain
** compilers were having problems with multiple definitions.
*/
#ifndef PREFAPI_H
typedef int (*PrefChangedFunc) (const char *, void *); 
#endif /* PREFAPI_H */

#define NS_IPREF_IID                                   \
  { /* {a22ad7b0-ca86-11d1-a9a4-00805f8a7ac4} */       \
    0xa22ad7b0,                                        \
    0xca86,                                            \
    0x11d1,                                            \
    { 0xa9, 0xa4, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } \
  }

#define NS_PREF_CID                                    \
  { /* {dc26e0e0-ca94-11d1-a9a4-00805f8a7ac4} */       \
    0xdc26e0e0,                                        \
    0xca94,                                            \
    0x11d1,                                            \
    { 0xa9, 0xa4, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } \
  }

/*
 * Return values
 */

#define NS_PREF_VALUE_CHANGED 1

class nsIPref: public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPREF_IID)

#ifdef PREF_SUPPORT_OLD_PATH_STRINGS
  nsresult   Shutdown() { return ShutDown(); }
  nsresult   Startup(void*) { return StartUp(); }
  NS_IMETHOD ReadUserJSFile(const char *filename) = 0; // deprecated
  NS_IMETHOD ReadLIJSFile(const char *filename) = 0; // deprecated
  NS_IMETHOD EvaluateConfigScript(const char * js_buffer, size_t length,
				  const char* filename, 
				  PRBool bGlobalContext, 
				  PRBool bCallbacks) = 0; // deprecated
  NS_IMETHOD SavePrefFileAs(const char *filename) = 0;
  NS_IMETHOD SaveLIPrefFile(const char *filename) = 0;

  // Path prefs
  NS_IMETHOD CopyPathPref(const char *pref, char ** return_buf) = 0;
  NS_IMETHOD SetPathPref(const char *pref_name, 
			 const char *path, PRBool set_default) = 0;
#endif
  // Initialize/shut down
  NS_IMETHOD StartUp() = 0;
  NS_IMETHOD StartUpWith(const nsFileSpec& inSpec) = 0;
  NS_IMETHOD ShutDown() = 0;

  // Config file input
  NS_IMETHOD ReadUserJSFile(const nsFileSpec& inSpec) = 0;
  NS_IMETHOD ReadLIJSFile(const nsFileSpec& inSpec) = 0;

  NS_IMETHOD EvaluateConfigScript(const char * js_buffer, size_t length,
				  PRBool bGlobalContext, 
				  PRBool bCallbacks) = 0;
  NS_IMETHOD EvaluateConfigScriptFile(const char * js_buffer, size_t length,
				  const nsFileSpec& inSpec, 
				  PRBool bGlobalContext, 
				  PRBool bCallbacks) = 0;

  NS_IMETHOD SavePrefFileAs(const nsFileSpec& inSpec) = 0;
  NS_IMETHOD SaveLIPrefFile(const nsFileSpec& inSpec) = 0;

  // JS stuff
  NS_IMETHOD GetConfigContext(JSContext **js_context) = 0;
  NS_IMETHOD GetGlobalConfigObject(JSObject **js_object) = 0;
  NS_IMETHOD GetPrefConfigObject(JSObject **js_object) = 0;

  // Getters
  NS_IMETHOD GetCharPref(const char *pref, 
			 char * return_buf, int * buf_length) = 0;
  NS_IMETHOD GetIntPref(const char *pref, PRInt32 * return_int) = 0;	
  NS_IMETHOD GetBoolPref(const char *pref, PRBool *return_val) = 0;	
  NS_IMETHOD GetBinaryPref(const char *pref, 
			 void * return_val, int * buf_length) = 0;	
  NS_IMETHOD GetColorPref(const char *pref_name,
			uint8 *red, uint8 *green, uint8 *blue) = 0;
  NS_IMETHOD GetColorPrefDWord(const char *pref_name, PRUint32 *colorref) = 0;
  NS_IMETHOD GetRectPref(const char *pref_name, 
			 PRInt16 *left, PRInt16 *top, 
			 PRInt16 *right, PRInt16 *bottom) = 0;

  // Setters
  NS_IMETHOD SetCharPref(const char *pref,const char* value) = 0;
  NS_IMETHOD SetIntPref(const char *pref,PRInt32 value) = 0;
  NS_IMETHOD SetBoolPref(const char *pref,PRBool value) = 0;
  NS_IMETHOD SetBinaryPref(const char *pref,void * value, long size) = 0;
  NS_IMETHOD SetColorPref(const char *pref_name, 
			  uint8 red, uint8 green, uint8 blue) = 0;
  NS_IMETHOD SetColorPrefDWord(const char *pref_name, PRUint32 colorref) = 0;
  NS_IMETHOD SetRectPref(const char *pref_name, 
			 PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom) = 0;
  NS_IMETHOD ClearUserPref(const char *pref_name) = 0;
  
  // Get Defaults
  NS_IMETHOD GetDefaultCharPref(const char *pref, 
				char * return_buf, int * buf_length) = 0;
  NS_IMETHOD GetDefaultIntPref(const char *pref, PRInt32 * return_int) = 0;
  NS_IMETHOD GetDefaultBoolPref(const char *pref, PRBool *return_val) = 0;
  NS_IMETHOD GetDefaultBinaryPref(const char *pref, 
				  void * return_val, int * buf_length) = 0;
  NS_IMETHOD GetDefaultColorPref(const char *pref_name, 
				 uint8 *red, uint8 *green, uint8 *blue) = 0;
  NS_IMETHOD GetDefaultColorPrefDWord(const char *pref_name, 
				      PRUint32 *colorref) = 0;
  NS_IMETHOD GetDefaultRectPref(const char *pref_name, 
				PRInt16 *left, PRInt16 *top, 
				PRInt16 *right, PRInt16 *bottom) = 0;

  // Set defaults
  NS_IMETHOD SetDefaultCharPref(const char *pref,const char* value) = 0;
  NS_IMETHOD SetDefaultIntPref(const char *pref,PRInt32 value) = 0;
  NS_IMETHOD SetDefaultBoolPref(const char *pref,PRBool value) = 0;
  NS_IMETHOD SetDefaultBinaryPref(const char *pref,
				  void * value, long size) = 0;
  NS_IMETHOD SetDefaultColorPref(const char *pref_name, 
				 uint8 red, uint8 green, uint8 blue) = 0;
  NS_IMETHOD SetDefaultRectPref(const char *pref_name, 
				PRInt16 left, PRInt16 top, 
				PRInt16 right, PRInt16 bottom) = 0;
  
  // Copy prefs
  NS_IMETHOD CopyCharPref(const char *pref, char ** return_buf) = 0;
  NS_IMETHOD CopyBinaryPref(const char *pref_name,
			void ** return_value, int *size) = 0;

  NS_IMETHOD CopyDefaultCharPref( const char 
				  *pref_name,  char ** return_buffer ) = 0;
  NS_IMETHOD CopyDefaultBinaryPref(const char *pref, 
				   void ** return_val, int * size) = 0;	

  NS_IMETHOD GetFilePref(const char* pref, nsFileSpec* value) = 0;
  NS_IMETHOD SetFilePref(
      const char* pref, const nsFileSpec* value, PRBool setDefault) = 0;

  // Pref info
  NS_IMETHOD PrefIsLocked(const char *pref_name, PRBool *res) = 0;

  // Save pref files
  NS_IMETHOD SavePrefFile() = 0;

  // Callbacks
  NS_IMETHOD RegisterCallback( const char* domain,
			       PrefChangedFunc callback, 
			       void* instance_data ) = 0;
  NS_IMETHOD UnregisterCallback( const char* domain,
				 PrefChangedFunc callback, 
				 void* instance_data ) = 0;

  NS_IMETHOD CopyPrefsTree(const char *srcRoot, const char *destRoot) = 0;
  NS_IMETHOD DeleteBranch(const char *branchName) = 0;
};

#endif /* nsIPref_h__ */
