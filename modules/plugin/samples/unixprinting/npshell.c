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
 *   Stephen Mak <smak@sun.com>
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

/*
 * npshell.c
 *
 * Netscape Client Plugin API
 * - Function that need to be implemented by plugin developers
 *
 * This file defines a "shell" plugin that plugin developers can use
 * as the basis for a real plugin.  This shell just provides empty
 * implementations of all functions that the plugin can implement
 * that will be called by Netscape (the NPP_xxx methods defined in
 * npapi.h).
 *
 * dp Suresh <dp@netscape.com>
 * updated 5/1998 <pollmann@netscape.com>
 * updated 9/2000 <smak@sun.com>
 * updated 3/2004 <dantifer.dang@sun.com>
 *
 */

#include <stdio.h>
#include <string.h>
#include "npapi.h"
#include "printplugin.h"
#include "strings.h"
#include "plstr.h"

/***********************************************************************
 *
 * Implementations of plugin API functions
 *
 ***********************************************************************/

char*
NPP_GetMIMEDescription(void)
{
    return(MIME_TYPES_HANDLED);
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    NPError err = NPERR_NO_ERROR;

    switch (variable) {
        case NPPVpluginNameString:
            *((char **)value) = PLUGIN_NAME;
            break;
        case NPPVpluginDescriptionString:
            *((char **)value) = PLUGIN_DESCRIPTION;
            break;
        default:
            err = NPERR_GENERIC_ERROR;
    }
    return err;
}

NPError
NPP_Initialize(void)
{
    return NPERR_NO_ERROR;
}

#ifdef OJI
jref
NPP_GetJavaClass()
{
    return NULL;
}
#endif

void
NPP_Shutdown(void)
{
}

NPError
NPP_New(NPMIMEType pluginType,
    NPP instance,
    uint16 mode,
    int16 argc,
    char* argn[],
    char* argv[],
    NPSavedData* saved)
{

    PluginInstance* This;

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    instance->pdata = NPN_MemAlloc(sizeof(PluginInstance));

    This = (PluginInstance*) instance->pdata;

    if (This == NULL)
    {
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    memset(This, 0, sizeof(PluginInstance));

    /* mode is NP_EMBED, NP_FULL, or NP_BACKGROUND (see npapi.h) */
    This->mode = mode;
    This->type = dupMimeType(pluginType);
    This->instance = instance;
    This->pluginsPrintMessage = NULL;
    This->exists = FALSE;

    /* Parse argument list passed to plugin instance */
    /* We are interested in these arguments
     *  PLUGINSPAGE = <url>
     */
    while (argc > 0)
    {
        argc --;
        if (argv[argc] != NULL)
        {
        if (!PL_strcasecmp(argn[argc], "PRINTMSG"))
            This->pluginsPrintMessage = strdup(argv[argc]);
        else if (!PL_strcasecmp(argn[argc], "HIDDEN"))
            This->pluginsHidden = (!PL_strcasecmp(argv[argc],
            "TRUE"));
        }
    }

    return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save)
{

    PluginInstance* This;

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    if (This != NULL) {
        if (This->type)
            NPN_MemFree(This->type);
        if (This->pluginsPrintMessage)
            NPN_MemFree(This->pluginsPrintMessage);
        NPN_MemFree(instance->pdata);
        instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
}


NPError
NPP_SetWindow(NPP instance, NPWindow* window)
{
    PluginInstance* This;
    NPSetWindowCallbackStruct *ws_info;

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    This = (PluginInstance*) instance->pdata;

    if (This == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    ws_info = (NPSetWindowCallbackStruct *)window->ws_info;

#ifdef MOZ_X11
    if (This->window == (Window) window->window) {
        /* The page with the plugin is being resized.
           Save any UI information because the next time
           around expect a SetWindow with a new window
           id.
        */
#ifdef DEBUG
        fprintf(stderr, "Printplugin: plugin received window resize.\n");
        fprintf(stderr, "Window=(%i)\n", (int)window);
        if (window) {
           fprintf(stderr, "W=(%i) H=(%i)\n",
               (int)window->width, (int)window->height);
        }
#endif
        return NPERR_NO_ERROR;
    } else {

      This->window = (Window) window->window;
      This->x = window->x;
      This->y = window->y;
      This->width = window->width;
      This->height = window->height;
      This->display = ws_info->display;
      This->visual = ws_info->visual;
      This->depth = ws_info->depth;
      This->colormap = ws_info->colormap;
      printScreenMessage(This);
    }
#endif  /* #ifdef MOZ_X11 */

    return NPERR_NO_ERROR;
}


NPError
NPP_NewStream(NPP instance,
          NPMIMEType type,
          NPStream *stream,
          NPBool seekable,
          uint16 *stype)
{
    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    return NPERR_NO_ERROR;
}


int32
NPP_WriteReady(NPP instance, NPStream *stream)
{
    /***** Insert NPP_WriteReady code here *****\
    PluginInstance* This;
    if (instance != NULL)
        This = (PluginInstance*) instance->pdata;
    \*******************************************/

    /* Number of bytes ready to accept in NPP_Write() */
    return -1L;   /* don't accept any bytes in NPP_Write() */
}


int32
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
    /***** Insert NPP_Write code here *****\
    PluginInstance* This;

    if (instance != NULL)
        This = (PluginInstance*) instance->pdata;
    \**************************************/

    return -1;   /* tell the browser to abort the stream, don't need it */
}


NPError
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    /***** Insert NPP_DestroyStream code here *****\
    PluginInstance* This;
    This = (PluginInstance*) instance->pdata;
    \**********************************************/

    return NPERR_NO_ERROR;
}


void
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
    /***** Insert NPP_StreamAsFile code here *****\
    PluginInstance* This;
    if (instance != NULL)
        This = (PluginInstance*) instance->pdata;
    \*********************************************/
}


void
NPP_URLNotify(NPP instance, const char* url,
                NPReason reason, void* notifyData)
{
    /***** Insert NPP_URLNotify code here *****\
    PluginInstance* This;
    if (instance != NULL)
        This = (PluginInstance*) instance->pdata;
    \*********************************************/
}


void
NPP_Print(NPP instance, NPPrint* printInfo)
{
    if(printInfo == NULL)
        return;

    if (instance != NULL) {
    /***** Insert NPP_Print code here *****\
        PluginInstance* This = (PluginInstance*) instance->pdata;
    \**************************************/

        if (printInfo->mode == NP_FULL) {
            /*
             * PLUGIN DEVELOPERS:
             *  If your plugin would like to take over
             *  printing completely when it is in full-screen mode,
             *  set printInfo->pluginPrinted to TRUE and print your
             *  plugin as you see fit.  If your plugin wants Netscape
             *  to handle printing in this case, set
             *  printInfo->pluginPrinted to FALSE (the default) and
             *  do nothing.  If you do want to handle printing
             *  yourself, printOne is true if the print button
             *  (as opposed to the print menu) was clicked.
             *  On the Macintosh, platformPrint is a THPrint; on
             *  Windows, platformPrint is a structure
             *  (defined in npapi.h) containing the printer name, port,
             *  etc.
             */

    /***** Insert NPP_Print code here *****\
            void* platformPrint =
                printInfo->print.fullPrint.platformPrint;
            NPBool printOne =
                printInfo->print.fullPrint.printOne;
    \**************************************/

            /* Do the default*/
            printInfo->print.fullPrint.pluginPrinted = FALSE;
        }
        else {  /* If not fullscreen, we must be embedded */
            /*
             * PLUGIN DEVELOPERS:
             *  If your plugin is embedded, or is full-screen
             *  but you returned false in pluginPrinted above, NPP_Print
             *  will be called with mode == NP_EMBED.  The NPWindow
             *  in the printInfo gives the location and dimensions of
             *  the embedded plugin on the printed page.  On the
             *  Macintosh, platformPrint is the printer port; on
             *  Windows, platformPrint is the handle to the printing
             *  device context.
             */

    /***** Insert NPP_Print code here *****\
            NPWindow* printWindow =
                &(printInfo->print.embedPrint.window);
            void* platformPrint =
                printInfo->print.embedPrint.platformPrint;
    \**************************************/
            PluginInstance* This;
            NPPrintCallbackStruct* platformPrint;
            FILE *output;

            platformPrint =
                (NPPrintCallbackStruct *)(printInfo->print.embedPrint.platformPrint);

            output = (FILE*)(platformPrint->fp);
            if (output == NULL)
                return;

            
            This =  (PluginInstance*) instance->pdata;
            if (This == NULL)
                return;

            printEPSMessage(This, output, printInfo->print.embedPrint.window);

        }
    }
}
