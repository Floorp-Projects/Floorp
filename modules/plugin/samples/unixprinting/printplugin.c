/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Dantifer Dang <dantifer.dang@sun.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@nrubsig.org>
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

/* Xlib/Xt stuff */
#ifdef MOZ_X11
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#endif /* MOZ_X11 */

#include "npapi.h"
#include "printplugin.h"
#include "prprf.h"

#include <stdlib.h>
#include <stdio.h>

/* MIMETypeList maintenance routines */
NPMIMEType
dupMimeType(NPMIMEType type)
{
    NPMIMEType mimetype = NPN_MemAlloc(strlen(type)+1);
    if (mimetype)
        strcpy(mimetype, type);
    return mimetype;
}

static void
privatePrintScreenMessage(PluginInstance *This)
{
#ifdef MOZ_X11
    GC gc;
    unsigned int h,w;
    int x,y,l;
    const char *string;
    gc = XCreateGC(This->display, This->window, 0, NULL);

    /* draw a rectangle */
    h = This->height/2;
    w = 3 * This->width/4;
    x = (This->width - w)/2; /* center */
    y = h/2;
    XDrawRectangle(This->display, This->window, gc, x, y, w, h);

    /* draw a string */
    string = This->pluginsPrintMessage;
    if (string && *string)
    {
        l = strlen(string);
        x += This->width/10;
        XDrawString(This->display, This->window, gc, x, This->height/2, string, l);
    }
    XFreeGC(This->display, gc);
#endif /* MOZ_X11 */
}

static void
setCursor (PluginInstance *This)
{
#ifdef MOZ_X11
    static Cursor nullPluginCursor = None;
    if (!nullPluginCursor)
    {
        nullPluginCursor = XCreateFontCursor(This->display, XC_hand2);
    }
    if (nullPluginCursor)
    {
        XDefineCursor(This->display, This->window, nullPluginCursor);
    }
#endif /* MOZ_X11 */
}

#ifdef MOZ_X11
static void
xt_event_handler(Widget xt_w, PluginInstance *This, XEvent *xevent, Boolean *b)
{
    switch (xevent->type)
    {
        case Expose:
            /* get rid of all other exposure events */
            while(XCheckTypedWindowEvent(This->display, This->window, Expose, xevent));
                privatePrintScreenMessage(This);
            break;
        case ButtonRelease:
            break;
        default:
            break;
    }
}
#endif /* MOZ_X11 */

static void
addXtEventHandler(PluginInstance *This)
{
#ifdef MOZ_X11
     Display *dpy = (Display*) This->display;
     Window xwin = (Window) This->window;
     Widget xt_w = XtWindowToWidget(dpy, xwin);
     if (xt_w)
     {
         long event_mask = ExposureMask | ButtonReleaseMask | ButtonPressMask;
         XSelectInput(dpy, xwin, event_mask);
         XtAddEventHandler(xt_w, event_mask, False, (XtEventHandler)xt_event_handler, This);
     }
#endif /* MOZ_X11 */
}

void printScreenMessage(PluginInstance *This)
{
    privatePrintScreenMessage(This);
    setCursor(This);
    addXtEventHandler(This);
}


void printEPSMessage(PluginInstance *This, FILE *output, NPWindow window)
{
    char *string;
    int x,y,h,w;
    
    if (!output)
      return;

    fprintf(output, "%%!PS-Adobe-3.0 EPSF-3.0\n");
    fprintf(output, "%%%%BoundingBox: 0 0 %d %d\n", window.width, window.height);
    fprintf(output, "%%%%EndComments\n");
    fprintf(output, "gsave\n");

    w = 3 * window.width  / 4;
    h =     window.height / 2;
    x = (window.width - w)/2; /* center */
    y = h/2;

    /* draw a rectangle */
    fprintf(output, "newpath\n");
    fprintf(output, "%d %d moveto 0 %d rlineto %d 0 rlineto 0 %d rlineto\n",
            x, y, h, w, -h);
    fprintf(output, "closepath\n");
    fprintf(output, "stroke\n");

    /* draw a string */
    string = This->pluginsPrintMessage;
    if (string && *string)
    {
        fprintf(output, "/Times-Roman findfont 300 scalefont setfont\n");
        fprintf(output, "%d %d moveto\n", x + window.width/10, window.height / 2);
        fprintf(output, "(%s) show\n",string);
    }

    fprintf(output,"grestore\n");
    fprintf(output,"%%%%EOF\n");
}

