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
#define PLUGIN_NAME         "Default Plugin"
#define PLUGIN_DESCRIPTION  "The default plugin handles plugin data for mimetypes and extensions that are not specified and facilitates downloading of new plugins."
#define CLICK_TO_GET        "Click here to get the plugin"       
#define CLICK_WHEN_DONE     "Click here after installing the plugin"

#define REFRESH_PLUGIN_LIST "javascript:navigator.plugins.refresh(true)"
#define PLUGINSPAGE_URL     "http://plugins.netscape.com/plug-in_finder.adp" /* XXX Branding: make configurable via .properties or prefs */
#define OK_BUTTON           "OK"
#define CANCEL_BUTTON       "CANCEL"
#if defined(HPUX)
#define JVM_SMARTUPDATE_URL "http://www.hp.com/go/java"
#elif defined(VMS)
#define JVM_SMARTUPDATE_URL "http://www.openvms.compaq.com/openvms/products/ips/mozilla_relnotes.html#java"
#else 
#define JVM_SMARTUPDATE_URL "http://home.netscape.com/plugins/jvm.html" /* XXX Branding: see above */
#endif /* HPUX */
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
#ifdef MOZ_X11
    Window window;
    Display *display;
#endif
    uint32 x, y;
    uint32 width, height;
    NPMIMEType type;
    char *message;

    NPP instance;
    char *pluginsPageUrl;
    char *pluginsFileUrl;
    NPBool pluginsHidden;
#ifdef MOZ_X11
    Visual* visual;
    Colormap colormap;
#endif
    unsigned int depth;
    GtkWidget* dialogBox;

    NPBool exists;  /* Does the widget already exist? */
    int action;     /* What action should we take? (GET or REFRESH) */

} PluginInstance;


typedef struct _MimeTypeElement
{
    PluginInstance *pinst;
    struct _MimeTypeElement *next;
} MimeTypeElement;

/* Extern functions */
extern void makeWidget(PluginInstance *This);
extern NPMIMEType dupMimeType(NPMIMEType type);
extern void destroyWidget(PluginInstance *This);
extern void makePixmap(PluginInstance *This);
extern void destroyPixmap();


