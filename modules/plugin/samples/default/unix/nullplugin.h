/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Stephen Mak <smak@sun.com>
 */

/*
 * nullplugin.h
 *
 * Implementation of the null plugins for Unix.
 *
 * dp <dp@netscape.com>
 * updated 5/1998 <pollmann@netscape.com> 
 * updated 9/2000 <smak@sun.com>
 *
 */

#define TARGET              "_blank"
#define MIME_TYPES_HANDLED  "*:.*:All types"
#define PLUGIN_NAME         "Netscape Default Plugin"
#define PLUGIN_DESCRIPTION  "The default plugin handles plugin data for mimetypes and extensions that are not specified and facilitates downloading of new plugins."
#define CLICK_TO_GET        "Click here to get the plugin"       
#define CLICK_WHEN_DONE     "Click here after installing the plugin"

#define REFRESH_PLUGIN_LIST "javascript:navigator.plugins.refresh(true)"
#define PLUGINSPAGE_URL     "http://cgi.netscape.com/cgi-bin/plug-in_finder.cgi"
#define OK_BUTTON           "OK"
#define CANCEL_BUTTON       "CANCEL"
#define JVM_SMARTUPDATE_URL "http://home.netscape.com/plugins/jvm.html" 
#define JVM_MINETYPE        "application/x-java-vm"
#define MESSAGE "\
This page contains information of a type (%s) that can\n\
only be viewed with the appropriate Plug-in.\n\
\n\
Click OK to download Plugin."

#define GET 1
#define REFRESH 2
#include <gtk/gtk.h>

typedef struct _PluginInstance
{
    uint16 mode;
    Window window;
    Display *display;
    uint32 x, y;
    uint32 width, height;
    NPMIMEType type;
    char *message;

    NPP instance;
    char *pluginsPageUrl;
    char *pluginsFileUrl;
    NPBool pluginsHidden;
    Visual* visual;
    Colormap colormap;
    unsigned int depth;
    GtkWidget* dialogBox;

    NPBool exists;  /* Does the widget already exist? */
    int action;     /* What action should we take? (GET or REFRESH) */

} PluginInstance;


typedef struct _MimeTypeElement
{
    NPMIMEType value;
    struct _MimeTypeElement *next;
} MimeTypeElement;

/* Extern functions */
extern void makeWidget(PluginInstance *This);
extern NPMIMEType dupMimeType(NPMIMEType type);
