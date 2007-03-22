/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Stephen Mak <smak@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * npunix.c
 *
 * Netscape Client Plugin API
 * - Wrapper function to interface with the Netscape Navigator
 *
 * dp Suresh <dp@netscape.com>
 *
 *----------------------------------------------------------------------
 * PLUGIN DEVELOPERS:
 *  YOU WILL NOT NEED TO EDIT THIS FILE.
 *----------------------------------------------------------------------
 */

#define XP_UNIX 1

#include <stdio.h>
#include "npapi.h"
#include "npupp.h"

/*
 * Define PLUGIN_TRACE to have the wrapper functions print
 * messages to stderr whenever they are called.
 */

#ifdef PLUGIN_TRACE
#include <stdio.h>
#define PLUGINDEBUGSTR(msg) fprintf(stderr, "%s\n", msg)
#else
#define PLUGINDEBUGSTR(msg)
#endif


/***********************************************************************
 *
 * Globals
 *
 ***********************************************************************/

static NPNetscapeFuncs   gNetscapeFuncs;    /* Netscape Function table */


/***********************************************************************
 *
 * Wrapper functions : plugin calling Netscape Navigator
 *
 * These functions let the plugin developer just call the APIs
 * as documented and defined in npapi.h, without needing to know
 * about the function table and call macros in npupp.h.
 *
 ***********************************************************************/

void
NPN_Version(int* plugin_major, int* plugin_minor,
         int* netscape_major, int* netscape_minor)
{
    *plugin_major = NP_VERSION_MAJOR;
    *plugin_minor = NP_VERSION_MINOR;

    /* Major version is in high byte */
    *netscape_major = gNetscapeFuncs.version >> 8;
    /* Minor version is in low byte */
    *netscape_minor = gNetscapeFuncs.version & 0xFF;
}

NPError
NPN_GetValue(NPP instance, NPNVariable variable, void *r_value)
{
    return CallNPN_GetValueProc(gNetscapeFuncs.getvalue,
                    instance, variable, r_value);
}

NPError
NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
    return CallNPN_SetValueProc(gNetscapeFuncs.setvalue,
                    instance, variable, value);
}

NPError
NPN_GetURL(NPP instance, const char* url, const char* window)
{
    return CallNPN_GetURLProc(gNetscapeFuncs.geturl, instance, url, window);
}

NPError
NPN_GetURLNotify(NPP instance, const char* url, const char* window, void* notifyData)
{
    return CallNPN_GetURLNotifyProc(gNetscapeFuncs.geturlnotify, instance, url, window, notifyData);
}

NPError
NPN_PostURL(NPP instance, const char* url, const char* window,
         uint32 len, const char* buf, NPBool file)
{
    return CallNPN_PostURLProc(gNetscapeFuncs.posturl, instance,
                    url, window, len, buf, file);
}

NPError
NPN_PostURLNotify(NPP instance, const char* url, const char* window, uint32 len,
                  const char* buf, NPBool file, void* notifyData)
{
    return CallNPN_PostURLNotifyProc(gNetscapeFuncs.posturlnotify,
            instance, url, window, len, buf, file, notifyData);
}

NPError
NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
    return CallNPN_RequestReadProc(gNetscapeFuncs.requestread,
                    stream, rangeList);
}

NPError
NPN_NewStream(NPP instance, NPMIMEType type, const char *window,
          NPStream** stream_ptr)
{
    return CallNPN_NewStreamProc(gNetscapeFuncs.newstream, instance,
                    type, window, stream_ptr);
}

int32
NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer)
{
    return CallNPN_WriteProc(gNetscapeFuncs.write, instance,
                    stream, len, buffer);
}

NPError
NPN_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
    return CallNPN_DestroyStreamProc(gNetscapeFuncs.destroystream,
                        instance, stream, reason);
}

void
NPN_Status(NPP instance, const char* message)
{
    CallNPN_StatusProc(gNetscapeFuncs.status, instance, message);
}

const char*
NPN_UserAgent(NPP instance)
{
    return CallNPN_UserAgentProc(gNetscapeFuncs.uagent, instance);
}

void*
NPN_MemAlloc(uint32 size)
{
    return CallNPN_MemAllocProc(gNetscapeFuncs.memalloc, size);
}

void NPN_MemFree(void* ptr)
{
    CallNPN_MemFreeProc(gNetscapeFuncs.memfree, ptr);
}

uint32 NPN_MemFlush(uint32 size)
{
    return CallNPN_MemFlushProc(gNetscapeFuncs.memflush, size);
}

void NPN_ReloadPlugins(NPBool reloadPages)
{
    CallNPN_ReloadPluginsProc(gNetscapeFuncs.reloadplugins, reloadPages);
}

#ifdef OJI
JRIEnv* NPN_GetJavaEnv()
{
    return CallNPN_GetJavaEnvProc(gNetscapeFuncs.getJavaEnv);
}

jref NPN_GetJavaPeer(NPP instance)
{
    return CallNPN_GetJavaPeerProc(gNetscapeFuncs.getJavaPeer,
                       instance);
}
#endif

void
NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
    CallNPN_InvalidateRectProc(gNetscapeFuncs.invalidaterect, instance,
        invalidRect);
}

void
NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
    CallNPN_InvalidateRegionProc(gNetscapeFuncs.invalidateregion, instance,
        invalidRegion);
}

void
NPN_ForceRedraw(NPP instance)
{
    CallNPN_ForceRedrawProc(gNetscapeFuncs.forceredraw, instance);
}

void NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
    CallNPN_PushPopupsEnabledStateProc(gNetscapeFuncs.pushpopupsenabledstate,
        instance, enabled);
}

void NPN_PopPopupsEnabledState(NPP instance)
{
    CallNPN_PopPopupsEnabledStateProc(gNetscapeFuncs.poppopupsenabledstate,
        instance);
}

NPIdentifier NPN_GetStringIdentifier(const NPUTF8 *name)
{
    return CallNPN_GetStringIdentifierProc(gNetscapeFuncs.getstringidentifier,
                                           name);
}

void NPN_GetStringIdentifiers(const NPUTF8 **names, int32_t nameCount,
                              NPIdentifier *identifiers)
{
    CallNPN_GetStringIdentifiersProc(gNetscapeFuncs.getstringidentifiers,
        names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier(int32_t intid)
{
    return CallNPN_GetIntIdentifierProc(gNetscapeFuncs.getintidentifier, intid);
}

bool NPN_IdentifierIsString(NPIdentifier identifier)
{
    return CallNPN_IdentifierIsStringProc(gNetscapeFuncs.identifierisstring,
        identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier(NPIdentifier identifier)
{
    return CallNPN_UTF8FromIdentifierProc(gNetscapeFuncs.utf8fromidentifier,
        identifier);
}

int32_t NPN_IntFromIdentifier(NPIdentifier identifier)
{
    return CallNPN_IntFromIdentifierProc(gNetscapeFuncs.intfromidentifier,
        identifier);
}

NPObject *NPN_CreateObject(NPP npp, NPClass *aClass)
{
    return CallNPN_CreateObjectProc(gNetscapeFuncs.createobject, npp, aClass);
}

NPObject *NPN_RetainObject(NPObject *obj)
{
    return CallNPN_RetainObjectProc(gNetscapeFuncs.retainobject, obj);
}

void NPN_ReleaseObject(NPObject *obj)
{
    CallNPN_ReleaseObjectProc(gNetscapeFuncs.releaseobject, obj);
}

bool NPN_Invoke(NPP npp, NPObject* obj, NPIdentifier methodName,
                const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    return CallNPN_InvokeProc(gNetscapeFuncs.invoke, npp, obj, methodName,
        args, argCount, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject* obj, const NPVariant *args,
                       uint32_t argCount, NPVariant *result)
{
    return CallNPN_InvokeDefaultProc(gNetscapeFuncs.invokeDefault, npp, obj,
        args, argCount, result);
}

bool NPN_Evaluate(NPP npp, NPObject* obj, NPString *script,
                  NPVariant *result)
{
    return CallNPN_EvaluateProc(gNetscapeFuncs.evaluate, npp, obj, script, result);
}

bool NPN_GetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                     NPVariant *result)
{
    return CallNPN_GetPropertyProc(gNetscapeFuncs.getproperty, npp, obj,
        propertyName, result);
}

bool NPN_SetProperty(NPP npp, NPObject* obj, NPIdentifier propertyName,
                     const NPVariant *value)
{
    return CallNPN_SetPropertyProc(gNetscapeFuncs.setproperty, npp, obj,
        propertyName, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    return CallNPN_RemovePropertyProc(gNetscapeFuncs.removeproperty, npp, obj,
        propertyName);
}

bool NPN_HasProperty(NPP npp, NPObject* obj, NPIdentifier propertyName)
{
    return CallNPN_HasPropertyProc(gNetscapeFuncs.hasproperty, npp, obj,
        propertyName);
}

bool NPN_HasMethod(NPP npp, NPObject* obj, NPIdentifier methodName)
{
    return CallNPN_HasMethodProc(gNetscapeFuncs.hasmethod, npp, obj, methodName);
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
    CallNPN_ReleaseVariantValueProc(gNetscapeFuncs.releasevariantvalue, variant);
}

void NPN_SetException(NPObject* obj, const NPUTF8 *message)
{
    CallNPN_SetExceptionProc(gNetscapeFuncs.setexception, obj, message);
}


/***********************************************************************
 *
 * Wrapper functions : Netscape Navigator -> plugin
 *
 * These functions let the plugin developer just create the APIs
 * as documented and defined in npapi.h, without needing to 
 * install those functions in the function table or worry about
 * setting up globals for 68K plugins.
 *
 ***********************************************************************/

NPError
Private_New(NPMIMEType pluginType, NPP instance, uint16 mode,
        int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
    NPError ret;
    PLUGINDEBUGSTR("New");
    ret = NPP_New(pluginType, instance, mode, argc, argn, argv, saved);
    return ret; 
}

NPError
Private_Destroy(NPP instance, NPSavedData** save)
{
    PLUGINDEBUGSTR("Destroy");
    return NPP_Destroy(instance, save);
}

NPError
Private_SetWindow(NPP instance, NPWindow* window)
{
    NPError err;
    PLUGINDEBUGSTR("SetWindow");
    err = NPP_SetWindow(instance, window);
    return err;
}

NPError
Private_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
            NPBool seekable, uint16* stype)
{
    NPError err;
    PLUGINDEBUGSTR("NewStream");
    err = NPP_NewStream(instance, type, stream, seekable, stype);
    return err;
}

int32
Private_WriteReady(NPP instance, NPStream* stream)
{
    unsigned int result;
    PLUGINDEBUGSTR("WriteReady");
    result = NPP_WriteReady(instance, stream);
    return result;
}

int32
Private_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
        void* buffer)
{
    unsigned int result;
    PLUGINDEBUGSTR("Write");
    result = NPP_Write(instance, stream, offset, len, buffer);
    return result;
}

void
Private_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
    PLUGINDEBUGSTR("StreamAsFile");
    NPP_StreamAsFile(instance, stream, fname);
}


NPError
Private_DestroyStream(NPP instance, NPStream* stream, NPError reason)
{
    NPError err;
    PLUGINDEBUGSTR("DestroyStream");
    err = NPP_DestroyStream(instance, stream, reason);
    return err;
}

void
Private_URLNotify(NPP instance, const char* url,
                NPReason reason, void* notifyData)
                
{
    PLUGINDEBUGSTR("URLNotify");
    NPP_URLNotify(instance, url, reason, notifyData);
}



void
Private_Print(NPP instance, NPPrint* platformPrint)
{
    PLUGINDEBUGSTR("Print");
    NPP_Print(instance, platformPrint);
}

#ifdef OJI
JRIGlobalRef
Private_GetJavaClass(void)
{
    jref clazz = NPP_GetJavaClass();
    if (clazz) {
    JRIEnv* env = NPN_GetJavaEnv();
    return JRI_NewGlobalRef(env, clazz);
    }
    return NULL;
}
#endif

/*********************************************************************** 
 *
 * These functions are located automagically by netscape.
 *
 ***********************************************************************/

/*
 * NP_GetMIMEDescription
 *  - Netscape needs to know about this symbol
 *  - Netscape uses the return value to identify when an object instance
 *    of this plugin should be created.
 */
char *
NP_GetMIMEDescription(void)
{
    return NPP_GetMIMEDescription();
}

/*
 * NP_GetValue [optional]
 *  - Netscape needs to know about this symbol.
 *  - Interfaces with plugin to get values for predefined variables
 *    that the navigator needs.
 */
NPError
NP_GetValue(void* future, NPPVariable variable, void *value)
{
    return NPP_GetValue(future, variable, value);
}

/*
 * NP_Initialize
 *  - Netscape needs to know about this symbol.
 *  - It calls this function after looking up its symbol before it
 *    is about to create the first ever object of this kind.
 *
 * PARAMETERS
 *    nsTable   - The netscape function table. If developers just use these
 *        wrappers, they don't need to worry about all these function
 *        tables.
 * RETURN
 *    pluginFuncs
 *      - This functions needs to fill the plugin function table
 *        pluginFuncs and return it. Netscape Navigator plugin
 *        library will use this function table to call the plugin.
 *
 */
NPError
NP_Initialize(NPNetscapeFuncs* nsTable, NPPluginFuncs* pluginFuncs)
{
    NPError err = NPERR_NO_ERROR;

    PLUGINDEBUGSTR("NP_Initialize");
    
    /* validate input parameters */

    if ((nsTable == NULL) || (pluginFuncs == NULL))
        err = NPERR_INVALID_FUNCTABLE_ERROR;
    
    /*
     * Check the major version passed in Netscape's function table.
     * We won't load if the major version is newer than what we expect.
     * Also check that the function tables passed in are big enough for
     * all the functions we need (they could be bigger, if Netscape added
     * new APIs, but that's OK with us -- we'll just ignore them).
     *
     */

    if (err == NPERR_NO_ERROR) {
        if ((nsTable->version >> 8) > NP_VERSION_MAJOR)
            err = NPERR_INCOMPATIBLE_VERSION_ERROR;
        if (nsTable->size < ((void *)&nsTable->posturlnotify - (void *)nsTable))
            err = NPERR_INVALID_FUNCTABLE_ERROR;
        if (pluginFuncs->size < sizeof(NPPluginFuncs))      
            err = NPERR_INVALID_FUNCTABLE_ERROR;
    }
        
    
    if (err == NPERR_NO_ERROR) {
        /*
         * Copy all the fields of Netscape function table into our
         * copy so we can call back into Netscape later.  Note that
         * we need to copy the fields one by one, rather than assigning
         * the whole structure, because the Netscape function table
         * could actually be bigger than what we expect.
         */
        gNetscapeFuncs.version       = nsTable->version;
        gNetscapeFuncs.size          = nsTable->size;
        gNetscapeFuncs.posturl       = nsTable->posturl;
        gNetscapeFuncs.geturl        = nsTable->geturl;
        gNetscapeFuncs.geturlnotify  = nsTable->geturlnotify;
        gNetscapeFuncs.requestread   = nsTable->requestread;
        gNetscapeFuncs.newstream     = nsTable->newstream;
        gNetscapeFuncs.write         = nsTable->write;
        gNetscapeFuncs.destroystream = nsTable->destroystream;
        gNetscapeFuncs.status        = nsTable->status;
        gNetscapeFuncs.uagent        = nsTable->uagent;
        gNetscapeFuncs.memalloc      = nsTable->memalloc;
        gNetscapeFuncs.memfree       = nsTable->memfree;
        gNetscapeFuncs.memflush      = nsTable->memflush;
        gNetscapeFuncs.reloadplugins = nsTable->reloadplugins;
#ifdef OJI
        gNetscapeFuncs.getJavaEnv    = nsTable->getJavaEnv;
        gNetscapeFuncs.getJavaPeer   = nsTable->getJavaPeer;
#endif
        gNetscapeFuncs.getvalue      = nsTable->getvalue;
        gNetscapeFuncs.setvalue      = nsTable->setvalue;
        gNetscapeFuncs.posturlnotify = nsTable->posturlnotify;

        if (nsTable->size >= ((void *)&nsTable->setexception - (void *)nsTable))
        {
          gNetscapeFuncs.invalidaterect = nsTable->invalidaterect;
          gNetscapeFuncs.invalidateregion = nsTable->invalidateregion;
          gNetscapeFuncs.forceredraw = nsTable->forceredraw;
          gNetscapeFuncs.getstringidentifier = nsTable->getstringidentifier;
          gNetscapeFuncs.getstringidentifiers = nsTable->getstringidentifiers;
          gNetscapeFuncs.getintidentifier = nsTable->getintidentifier;
          gNetscapeFuncs.identifierisstring = nsTable->identifierisstring;
          gNetscapeFuncs.utf8fromidentifier = nsTable->utf8fromidentifier;
          gNetscapeFuncs.intfromidentifier = nsTable->intfromidentifier;
          gNetscapeFuncs.createobject = nsTable->createobject;
          gNetscapeFuncs.retainobject = nsTable->retainobject;
          gNetscapeFuncs.releaseobject = nsTable->releaseobject;
          gNetscapeFuncs.invoke = nsTable->invoke;
          gNetscapeFuncs.invokeDefault = nsTable->invokeDefault;
          gNetscapeFuncs.evaluate = nsTable->evaluate;
          gNetscapeFuncs.getproperty = nsTable->getproperty;
          gNetscapeFuncs.setproperty = nsTable->setproperty;
          gNetscapeFuncs.removeproperty = nsTable->removeproperty;
          gNetscapeFuncs.hasproperty = nsTable->hasproperty;
          gNetscapeFuncs.hasmethod = nsTable->hasmethod;
          gNetscapeFuncs.releasevariantvalue = nsTable->releasevariantvalue;
          gNetscapeFuncs.setexception = nsTable->setexception;
        }
         else
        {
          gNetscapeFuncs.invalidaterect = NULL;
          gNetscapeFuncs.invalidateregion = NULL;
          gNetscapeFuncs.forceredraw = NULL;
          gNetscapeFuncs.getstringidentifier = NULL;
          gNetscapeFuncs.getstringidentifiers = NULL;
          gNetscapeFuncs.getintidentifier = NULL;
          gNetscapeFuncs.identifierisstring = NULL;
          gNetscapeFuncs.utf8fromidentifier = NULL;
          gNetscapeFuncs.intfromidentifier = NULL;
          gNetscapeFuncs.createobject = NULL;
          gNetscapeFuncs.retainobject = NULL;
          gNetscapeFuncs.releaseobject = NULL;
          gNetscapeFuncs.invoke = NULL;
          gNetscapeFuncs.invokeDefault = NULL;
          gNetscapeFuncs.evaluate = NULL;
          gNetscapeFuncs.getproperty = NULL;
          gNetscapeFuncs.setproperty = NULL;
          gNetscapeFuncs.removeproperty = NULL;
          gNetscapeFuncs.hasproperty = NULL;
          gNetscapeFuncs.releasevariantvalue = NULL;
          gNetscapeFuncs.setexception = NULL;
        }
        if (nsTable->size >=
            ((void *)&nsTable->poppopupsenabledstate - (void *)nsTable))
        {
          gNetscapeFuncs.pushpopupsenabledstate = nsTable->pushpopupsenabledstate;
          gNetscapeFuncs.poppopupsenabledstate  = nsTable->poppopupsenabledstate;
        }
         else
        {
          gNetscapeFuncs.pushpopupsenabledstate = NULL;
          gNetscapeFuncs.poppopupsenabledstate  = NULL;
        }

        /*
         * Set up the plugin function table that Netscape will use to
         * call us.  Netscape needs to know about our version and size
         * and have a UniversalProcPointer for every function we
         * implement.
         */
        pluginFuncs->version    = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
        pluginFuncs->size       = sizeof(NPPluginFuncs);
        pluginFuncs->newp       = NewNPP_NewProc(Private_New);
        pluginFuncs->destroy    = NewNPP_DestroyProc(Private_Destroy);
        pluginFuncs->setwindow  = NewNPP_SetWindowProc(Private_SetWindow);
        pluginFuncs->newstream  = NewNPP_NewStreamProc(Private_NewStream);
        pluginFuncs->destroystream = NewNPP_DestroyStreamProc(Private_DestroyStream);
        pluginFuncs->asfile     = NewNPP_StreamAsFileProc(Private_StreamAsFile);
        pluginFuncs->writeready = NewNPP_WriteReadyProc(Private_WriteReady);
        pluginFuncs->write      = NewNPP_WriteProc(Private_Write);
        pluginFuncs->print      = NewNPP_PrintProc(Private_Print);
        pluginFuncs->urlnotify  = NewNPP_URLNotifyProc(Private_URLNotify);
        pluginFuncs->event      = NULL;
#ifdef OJI
        pluginFuncs->javaClass  = Private_GetJavaClass();
#endif
        // This function is supposedly loaded magically, but that doesn't
        // seem to be true.
        pluginFuncs->getvalue      = NewNPP_GetValueProc(NP_GetValue);

        err = NPP_Initialize();
    }
    
    return err;
}

/*
 * NP_Shutdown [optional]
 *  - Netscape needs to know about this symbol.
 *  - It calls this function after looking up its symbol after
 *    the last object of this kind has been destroyed.
 *
 */
NPError
NP_Shutdown(void)
{
    PLUGINDEBUGSTR("NP_Shutdown");
    NPP_Shutdown();
    return NPERR_NO_ERROR;
}
