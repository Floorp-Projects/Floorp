/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "jri.h"

#ifndef _netscape_plugin_Plugin_H_
#define _netscape_plugin_Plugin_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct netscape_javascript_JSObject;
struct java_lang_Class;
struct netscape_plugin_Plugin;

/*******************************************************************************
 * Class netscape/plugin/Plugin
 ******************************************************************************/

typedef struct netscape_plugin_Plugin netscape_plugin_Plugin;

struct netscape_plugin_Plugin {
  void *ptr;
};

/*******************************************************************************
 * Public Methods
 ******************************************************************************/

JRI_PUBLIC_API(void)
netscape_plugin_Plugin_destroy(JRIEnv* env, struct netscape_plugin_Plugin* self) ;

JRI_PUBLIC_API(void)
netscape_plugin_Plugin_init(JRIEnv* env, struct netscape_plugin_Plugin* self) ;

JRI_PUBLIC_API(struct netscape_plugin_Plugin*)
netscape_plugin_Plugin_new(JRIEnv* env, struct java_lang_Class* clazz) ;

JRI_PUBLIC_API(void)
set_netscape_plugin_Plugin_peer(JRIEnv* env, struct netscape_plugin_Plugin* obj, jint v) ;

JRI_PUBLIC_API(void)
set_netscape_plugin_Plugin_window(JRIEnv* env, struct netscape_plugin_Plugin* obj, struct netscape_javascript_JSObject * v) ;

JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_Plugin(JRIEnv* env);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* Class netscape/plugin/Plugin */
