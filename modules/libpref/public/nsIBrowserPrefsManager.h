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

#ifndef nsIBrowserPrefsManager_h__
#define nsIBrowserPrefsManager_h__

#include "nsIPrefsManager.h"
#include "jsapi.h"
#include "nsISupports.h"

// Browser-specific interface to prefs

class nsIBrowserPrefsManager: public nsIPrefsManager {
public:
  // Initialize/shutdown
  NS_IMETHOD Startup(char *filename) = 0;
  NS_IMETHOD Shutdown() = 0;

  // Config file input
  NS_IMETHOD ReadUserJSFile(char *filename) = 0;
  NS_IMETHOD ReadLIJSFile(char *filename) = 0;

  // JS stuff
  NS_IMETHOD GetConfigContext(JSContext **js_context) = 0;
  NS_IMETHOD GetGlobalConfigObject(JSObject **js_object) = 0;
  NS_IMETHOD GetPrefConfigObject(JSObject **js_object) = 0;

  NS_IMETHOD EvaluateConfigScript(const char * js_buffer, size_t length,
				  const char* filename, 
				  PRBool bGlobalContext, 
				  PRBool bCallbacks) = 0;

  // Copy prefs
  NS_IMETHOD CopyCharPref(const char *pref, char ** return_buf) = 0;
  NS_IMETHOD CopyBinaryPref(const char *pref_name,
			void ** return_value, int *size) = 0;

  NS_IMETHOD CopyDefaultCharPref( const char 
				  *pref_name,  char ** return_buffer ) = 0;
  NS_IMETHOD CopyDefaultBinaryPref(const char *pref, 
				   void ** return_val, int * size) = 0;	

  // Path prefs
  NS_IMETHOD CopyPathPref(const char *pref, char ** return_buf) = 0;
  NS_IMETHOD SetPathPref(const char *pref_name, 
			 const char *path, PRBool set_default) = 0;

  // Save pref files
  NS_IMETHOD SavePrefFile(void) = 0;
  NS_IMETHOD SavePrefFileAs(const char *filename) = 0;
  NS_IMETHOD SaveLIPrefFile(const char *filename) = 0;

};

#endif /* nsIBrowserPrefsManager_h__ */
