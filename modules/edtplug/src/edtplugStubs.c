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

#ifndef XP_MAC
#define IMPLEMENT_netscape_plugin_composer_PluginManager
#define IMPLEMENT_netscape_plugin_composer_MozillaCallback
#define IMPLEMENT_netscape_plugin_composer_Composer
#include "_jri/netscape_plugin_composer_PluginManager.c"
#include "_jri/netscape_plugin_composer_MozillaCallback.c"
#include "_jri/netscape_plugin_composer_Composer.c"
#else
#define IMPLEMENT_netscape_plugin_composer_PluginManager
#define IMPLEMENT_netscape_plugin_composer_MozillaCallback
#define IMPLEMENT_netscape_plugin_composer_Composer
#include "n_p_composer_PluginManager.c"
#include "n_p_composer_MozillaCallback.c"
#include "n_plugin_composer_Composer.c"
#endif

#ifndef NSPR20
#include "prevent.h"
#else
#include "plevent.h"
#endif
#include "prmon.h"
#include "prmem.h"
#include "edtplug.h"

extern PREventQueue* mozilla_event_queue;

/* Dummy method used to ensure this code is linked into the main application */
void _java_edtplug_init(void) { }


/* Call back from plugin thread into the editor. */

/******************************************************************************/

typedef struct MozillaEvent_MozillaCallback {
    PREvent	event;
    JRIEnv* env;
    JRIGlobalRef this;
} MozillaEvent_MozillaCallback;

typedef void (*ComposerPluginCallback)(jint, jint, struct java_lang_Object*);

static EditURLCallback gEditURLCallback;

/* 97-05-06 pkc -- declare PR_PUBLIC_API on Mac to force export
 * out of shared library
 */
#ifdef XP_MAC
PR_PUBLIC_API(void) EDTPLUG_RegisterEditURLCallback(EditURLCallback callback){
    gEditURLCallback = callback;
}
#else
void EDTPLUG_RegisterEditURLCallback(EditURLCallback callback){
    gEditURLCallback = callback;
}
#endif /* XP_MAC */ 

static JRIEnv* getEnv(MozillaEvent_MozillaCallback* e)
{
    JRIEnv* ee = e->env;
#ifdef JAVA     // XXX hack to get OJI working
    if ( ! ee ) {
        ee = JRI_GetCurrentEnv();
    }
#endif
    return ee;
}

static int bMozillaCallbackInitialized = 0;

void PR_CALLBACK
edtplug_HandleEvent_MozillaCallback(MozillaEvent_MozillaCallback* e)
{
    JRIEnv* ee = getEnv(e);
    struct netscape_plugin_composer_MozillaCallback* obj =
        (struct netscape_plugin_composer_MozillaCallback*)
            JRI_GetGlobalRef(ee, e->this);
    /* If JavaScript calls netscape.plugin.composer.Document.editDocument(),
     * then this will be the first time that we've ever tried
     * to call into netscape_plugin_composer_MozillaCallback.
     * Therefore, we need to call the use function.
     * We know that it is fast and safe to call
     * multiple times.
     */
    if ( ! bMozillaCallbackInitialized ) {
        use_netscape_plugin_composer_MozillaCallback(ee);
        bMozillaCallbackInitialized = 1;
    }
    netscape_plugin_composer_MozillaCallback_perform(ee, obj);
}

void PR_CALLBACK
edtplug_DestroyEvent_MozillaCallback(MozillaEvent_MozillaCallback* event)
{
    JRIEnv* ee = getEnv(event);
    JRI_DisposeGlobalRef(ee, event->this);
    PR_DELETE(event);
}

JRI_PUBLIC_API(void)
native_netscape_plugin_composer_MozillaCallback_pEnqueue(
    JRIEnv* env,
    struct netscape_plugin_composer_MozillaCallback* object,
    jint mozenv)
{
    MozillaEvent_MozillaCallback* event;
    PR_ENTER_EVENT_QUEUE_MONITOR(mozilla_event_queue);

    event = PR_NEW(MozillaEvent_MozillaCallback);
	if (event == NULL) goto done;
    PR_InitEvent(&event->event, NULL,
				 (PRHandleEventProc)edtplug_HandleEvent_MozillaCallback,
				 (PRDestroyEventProc)edtplug_DestroyEvent_MozillaCallback);
    /* Tell Java not to garbage collect the object that's stored in this event */
    event->env = (JRIEnv*) mozenv;
    event->this = JRI_NewGlobalRef(env, object);

    PR_PostEvent(mozilla_event_queue, &event->event);

  done:
    PR_EXIT_EVENT_QUEUE_MONITOR(mozilla_event_queue);
}

JRI_PUBLIC_API(void)
native_netscape_plugin_composer_Composer_mtCallback(
    JRIEnv* env,
    netscape_plugin_composer_Composer* this,
    jint context,
    jint callback,
    jint action,
    struct java_lang_Object* arg)
{
    ComposerPluginCallback f;

    f = (ComposerPluginCallback) callback;

    if ( f ){
        (*f)(context, action, arg);
    }
    else {
        /* Static callbacks */
        switch(action ) {
            case 4: /* OpenEditor Must match Composer.PLUGIN_EDITURL */
                {
                    /* str is garbage collected by Java. */
                    const char* str = JRI_GetStringPlatformChars(env, (struct java_lang_String*)arg, "", 0);
                    if ( str ) {
                        if ( gEditURLCallback ) {
                            (*gEditURLCallback)(str);
                        }
                    }
                }
            break;
        }
    }
}
