/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "plevent.h"
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
