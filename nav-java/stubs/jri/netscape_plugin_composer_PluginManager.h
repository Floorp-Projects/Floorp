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

#ifndef _netscape_plugin_composer_PluginManager_H_
#define _netscape_plugin_composer_PluginManager_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct java_lang_String;
struct netscape_plugin_composer_Composer;
struct netscape_javascript_JSObject;
struct java_lang_Class;
struct netscape_plugin_composer_PluginManager;

/*******************************************************************************
 * Class netscape/plugin/composer/PluginManager
 ******************************************************************************/

typedef struct netscape_plugin_composer_PluginManager netscape_plugin_composer_PluginManager;
struct netscape_plugin_composer_PluginManager {
  void *ptr;
};

/*******************************************************************************
 * Public Methods
 ******************************************************************************/

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getCategoryName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderFileExtension(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderFileType(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderHint(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getEncoderName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_getEncoderNeedsQuantizedSource(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfCategories(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self);

JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfEncoders(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self) ;

JRI_PUBLIC_API(jint)
netscape_plugin_composer_PluginManager_getNumberOfPlugins(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getPluginHint(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a, jint b);

JRI_PUBLIC_API(struct java_lang_String *)
netscape_plugin_composer_PluginManager_getPluginName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, jint a, jint b);

JRI_PUBLIC_API(struct netscape_plugin_composer_PluginManager*)
netscape_plugin_composer_PluginManager_new(JRIEnv* env, struct java_lang_Class* clazz);

JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_performPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, jint b, jint c, struct java_lang_String *d, struct java_lang_String *e, struct java_lang_String *f, struct java_lang_String *g, struct netscape_javascript_JSObject *h);

JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_performPluginByName(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, struct java_lang_String *b, struct java_lang_String *c, struct java_lang_String *d, struct java_lang_String *e, struct java_lang_String *f, struct netscape_javascript_JSObject *g);

JRI_PUBLIC_API(void)
netscape_plugin_composer_PluginManager_registerPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct java_lang_String *a, struct java_lang_String *b);

JRI_PUBLIC_API(jbool)
netscape_plugin_composer_PluginManager_startEncoder(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a, jint b, jint c, jint d, jarrayArray e, struct java_lang_String *f);

JRI_PUBLIC_API(void)
netscape_plugin_composer_PluginManager_stopPlugin(JRIEnv* env, struct netscape_plugin_composer_PluginManager* self, struct netscape_plugin_composer_Composer *a);

JRI_PUBLIC_API(struct java_lang_Class*)
use_netscape_plugin_composer_PluginManager(JRIEnv* env);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* Class netscape/plugin/composer/PluginManager */

