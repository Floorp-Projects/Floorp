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
 * nullplugin.c
 *
 * Implementation of the null plugins for Unix.
 *
 * dp <dp@netscape.com>
 * updated 5/1998 <pollmann@netscape.com>
 * updated 9/2000 <smak@sun.com>
 *
 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

/* Xlib/Xt stuff */
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>

#include "npapi.h"
#include "nullplugin.h"
#include "prprf.h"

#define DIALOGID "dialog"

/* Global data */
static MimeTypeElement *head = NULL;

/* destroy the dialog box */
void
destroyWidget(PluginInstance *This)
{
    if (This && This->dialogBox)
    {
      gtk_widget_destroy (GTK_WIDGET(This->dialogBox));
    }
}

/* callback function for the OK button */
static void 
DialogOKClicked (GtkButton *button, gpointer data)
{
    PluginInstance* This = (PluginInstance*) data;
    GtkWidget* dialogWindow = gtk_object_get_data(GTK_OBJECT(button), DIALOGID);
    char *url;

    gtk_object_remove_data(GTK_OBJECT(button), DIALOGID);

    if (This->pluginsFileUrl != NULL)
    {
        /* Get the JavaScript command string */
        static const char buf[] = 
          "javascript:netscape.softupdate.Trigger.StartSoftwareUpdate(\"%s\")";

        url = NPN_MemAlloc(strlen(This->pluginsFileUrl) + (sizeof(buf) - 2));
        if (url != NULL)
        {
            /* Insert the file URL into the JavaScript command */
            sprintf(url, buf, This->pluginsFileUrl);
            NPN_GetURL(This->instance, url, TARGET);
            NPN_MemFree(url);
        }
    }
    else
    {
        /* If necessary, get the default plug-ins page resource */
        char* address = This->pluginsPageUrl;
        if (address == NULL || *address == 0)
        {
            address = PLUGINSPAGE_URL;
        }

        url = NPN_MemAlloc(strlen(address) + 1 + strlen(This->type)+1);
        if (url != NULL)
        {
                /* Append the MIME type to the URL */
            sprintf(url, "%s?%s", address, This->type);
            if (strcmp (This->type, JVM_MINETYPE) == 0) 
            {
                NPN_GetURL(This->instance, JVM_SMARTUPDATE_URL , TARGET);
            }
            else 
            {
                NPN_GetURL(This->instance, url, TARGET);
            }
            NPN_MemFree(url);
        }
    }
    destroyWidget(This);
}

/* the call back function for cancel button */
static void 
DialogCancelClicked (GtkButton *button, gpointer data) 
{
    destroyWidget((PluginInstance*) data);
}

/* function that closes the dialog if ESC is pressed */
static gboolean
DialogEscapePressed (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->keyval == GDK_Escape)
    {
        gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");
        gtk_object_destroy (GTK_OBJECT (widget));
        return TRUE;
    }

    return FALSE;
}

/* a handy procedure to add a widget and pack it as well */
static GtkWidget *
AddWidget (GtkWidget *widget, GtkWidget *packingbox)
{
    gtk_box_pack_start(GTK_BOX(packingbox), widget, TRUE, TRUE, 2);
    return widget;
}



/* MIMETypeList maintenance routines */

static gboolean
isEqual(NPMIMEType t1, NPMIMEType t2)
{
    return (t1 && t2) ? (strcmp(t1, t2) == 0) : FALSE; 
}

static MimeTypeElement * 
isExist(MimeTypeElement **typelist, NPMIMEType type)
{
    MimeTypeElement *ele;

    ele = *typelist;
    while (ele != NULL) {
        if (isEqual(ele->pinst->type, type))
            return ele;
        ele = ele->next;
    }
    return NULL;
}

NPMIMEType
dupMimeType(NPMIMEType type)
{
    NPMIMEType mimetype = NPN_MemAlloc(strlen(type)+1);
    if (mimetype)
        strcpy(mimetype, type);
    return(mimetype);
}

static gboolean 
addToList(MimeTypeElement **typelist, PluginInstance *This)
{
    if (This && This->type && !isExist(typelist, This->type))
    {
        MimeTypeElement *ele;
        if ((ele = (MimeTypeElement *) NPN_MemAlloc(sizeof(MimeTypeElement))))
        {
            ele->pinst = This;
            ele->next = *typelist;
            *typelist = ele;
            return(TRUE);
        }
    }
    return(FALSE);
}

static gboolean
delFromList(MimeTypeElement **typelist, PluginInstance *This)
{
    if (typelist && This)
    {
        MimeTypeElement *ele_prev;
        MimeTypeElement *ele = *typelist;
        while (ele)
        {
            if (isEqual(ele->pinst->type, This->type))
            {
                if (*typelist == ele)
                {
                    *typelist = ele->next;
                } else {
                    ele_prev->next = ele->next;
                }
                NPN_MemFree(ele);
                return(TRUE);
            }
            ele_prev = ele;
            ele = ele->next;
        }
    }
    return(FALSE);
}

static void
onDestroyWidget(GtkWidget *w, gpointer data)
{
    PluginInstance* This = (PluginInstance*) data;
    if (This && This->dialogBox && This->dialogBox == w)
    {
        This->dialogBox = 0;
        delFromList(&head, This);
    }
}

/* create and display the dialog box */
void 
makeWidget(PluginInstance *This)
{
    GtkWidget *dialogWindow;
    GtkWidget *dialogMessage;
    GtkWidget *okButton;
    GtkWidget *cancelButton;
    char message[1024];
    MimeTypeElement *ele;

    if (!This) return;

    /* need to check whether we already pop up a dialog box for previous
       minetype in the same web page. It's require otherwise there will
       be 2 dialog boxes pop if there are 2 plugin in the same web page
    */
    if ((ele = isExist(&head, This->type)))
    {
        if (ele->pinst && ele->pinst->dialogBox)
        {
            GtkWidget *top_window = gtk_widget_get_toplevel(ele->pinst->dialogBox);
            if (top_window && GTK_WIDGET_VISIBLE(top_window))
            {   /* this will raise the toplevel window */
                gdk_window_show(top_window->window);
            }
        }
        return;
    }

    dialogWindow = gtk_dialog_new();
    This->dialogBox = dialogWindow;
    This->exists = TRUE;
    This->dialogBox = dialogWindow;
    addToList(&head, This);
    gtk_window_set_title(GTK_WINDOW(dialogWindow), PLUGIN_NAME);
    gtk_window_set_position(GTK_WINDOW(dialogWindow), GTK_WIN_POS_CENTER);
    gtk_window_set_modal(GTK_WINDOW(dialogWindow), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(dialogWindow), 20);
    gtk_window_set_policy(GTK_WINDOW(dialogWindow), FALSE, FALSE, TRUE);

    PR_snprintf(message, sizeof(message) - 1, MESSAGE, This->type);
    dialogMessage = AddWidget(gtk_label_new (message), 
                   GTK_DIALOG(dialogWindow)->vbox);

    okButton= AddWidget(gtk_button_new_with_label (OK_BUTTON), 
                   GTK_DIALOG(dialogWindow)->action_area);
    gtk_object_set_data(GTK_OBJECT(okButton), DIALOGID, dialogWindow);

    GTK_WIDGET_SET_FLAGS (okButton, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(okButton);

    cancelButton= AddWidget(gtk_button_new_with_label (CANCEL_BUTTON), 
                   GTK_DIALOG(dialogWindow)->action_area);

    gtk_signal_connect (GTK_OBJECT(okButton),  "clicked",
                        GTK_SIGNAL_FUNC(DialogOKClicked), This);

    gtk_signal_connect (GTK_OBJECT(cancelButton),  "clicked",
                        GTK_SIGNAL_FUNC(DialogCancelClicked), This);

    gtk_signal_connect(GTK_OBJECT(dialogWindow), "key_press_event",
                        GTK_SIGNAL_FUNC (DialogEscapePressed), NULL);

    /* hookup to when the dialog is destroyed */
    gtk_signal_connect(GTK_OBJECT(dialogWindow), "destroy",
                        GTK_SIGNAL_FUNC(onDestroyWidget), This);

    gtk_widget_show_all(dialogWindow);
}

/* XPM */
static char * npnul320_xpm[] = {
"32 32 6 1",
"       c None",
".      c #808080",
"+      c #F8F8F8",
"@      c #C0C0C0",
"#      c #000000",
"$      c #00F8F8",
"........................++++++++",
".++++++++++++++++++++++..+++++++",
".+++++++++++++++++++++@.@.++++++",
".++@@@@@@@@@@@@@@@@@@@@.+@.+++++",
".++@@@@@@@@@.....@@@@@@.++@.++++",
".++@@@@@@@@.+++++#@@@@@.+++@.+++",
".++@@@@@@@.++$$$$$#@@@@.++++@.++",
".++@@@@@@@.+$$$$$$#.@@@.+++++@.+",
".++@@@...@@.+$$$$#..###.#######+",
".++@@.+++$$++$$$$$##++$#......#+",
".++@@.+$$$++$$$$$$$+$$$#......#+",
".++@@.+$$$$$$$$$$$$$$$$#..@@++#+",
".++@@@$$$$$$$$$$$$$$$$#...@@++#+",
".++@@@$#$##.$$$$$$##$$#...@@++#+",
".++@@@@##...$$$$$#..##...@@@++#+",
".++@@@@@....+$$$$#.......@@@++#+",
".++@@@@@@...+$$$$#.@@@..@@@@++#+",
".++@@@@..@@.+$$$$#.@##@@@@@@++#+",
".++@@@.++$$++$$$$$##$$#@@@@@++#+",
".++@@@.+$$++$$$$$$$$$$#@@@@@++#+",
".++@@.++$$$$$$$$$$$$$$$#@@@@++#+",
".++@@.+$$$$$$$$$$$$$$$$#.@@@++#+",
".++@@.+$$##$$$$$$$##$$$#..@@++#+",
".++@@@###...$$$$$#.@###...@@++#+",
".++@@@@....$$$$$$$#.@.....@@++#+",
".++@@@@@...$$$$$$$#..@...@@@++#+",
".++@@@@@@@@#$$$$$#...@@@@@@@++#+",
".++@@@@@@@@@#####...@@@@@@@@++#+",
".++@@@@@@@@@@......@@@@@@@@@++#+",
".+++++++++++++....++++++++++++#+",
".+++++++++++++++++++++++++++++#+",
"###############################+"};


static GdkPixmap *nullPluginGdkPixmap = 0;

static GdkWindow *getGdkWindow(PluginInstance *This)
{
    GdkWindow *gdk_window;
    Window xwin = (Window) This->window;
    Widget xt_w = XtWindowToWidget(This->display, xwin);

    if (xt_w) {
      xt_w = XtParent(xt_w);
      if (xt_w) {
         xwin = XtWindow(xt_w);
         /* xwin = xt_w->core.window; */
      }
    }
    gdk_window = gdk_window_lookup(xwin);
    return gdk_window;
}

static void
createPixmap(PluginInstance *This)
{
    int err = 0;

    if (nullPluginGdkPixmap == 0)
    { 
       GtkStyle *style;
       GdkBitmap *mask;
       GdkWindow *gdk_window = getGdkWindow(This);
       if (gdk_window)
       {
#ifdef MOZ_ENABLE_GTK
           style = gtk_widget_get_style((GtkWidget *)gdk_window->user_data);
#endif
#ifdef MOZ_ENABLE_GTK2
	   gpoint user_data = nsnull;
	   gdk_window_get_user_data( gdk_window, user_data);
	   style = gtk_widget_get_style(GTK_WIDGET(user_data));
#endif
           nullPluginGdkPixmap = gdk_pixmap_create_from_xpm_d(gdk_window , &mask,
                                             &style->bg[GTK_STATE_NORMAL], npnul320_xpm);
	   /* Pixmap is created on original X session but used by new session */
	   XSync(GDK_DISPLAY(), False);
       }
    }
}

static void
drawPixmap(PluginInstance *This)
{
    if (nullPluginGdkPixmap)
    {
        int pixmap_with, pixmap_height, dest_x, dest_y;
        gdk_window_get_size((GdkWindow *)nullPluginGdkPixmap, &pixmap_with, &pixmap_height);
        dest_x = This->width/2 - pixmap_with/2;
        dest_y = This->height/2 - pixmap_height/2;
        if (dest_x >= 0 && dest_y >= 0)
        {
            GC gc;
            gc = XCreateGC(This->display, This->window, 0, NULL);
            XCopyArea(This->display, GDK_WINDOW_XWINDOW(nullPluginGdkPixmap) , This->window, gc,
                0, 0, pixmap_with, pixmap_height, dest_x, dest_y);
            XFreeGC(This->display, gc);
        }
    }
}

static void
setCursor (PluginInstance *This)
{
    static Cursor nullPluginCursor = 0;
    if (!nullPluginCursor)
    {
        nullPluginCursor = XCreateFontCursor(This->display, XC_hand2);
    }
    if (nullPluginCursor)
    {
        XDefineCursor(This->display, This->window, nullPluginCursor);
    }
}

static void
xt_event_handler(Widget xt_w, PluginInstance *This, XEvent *xevent, Boolean *b)
{
    switch (xevent->type)
    {
        case Expose:
            /* get rid of all other exposure events */
            while(XCheckTypedWindowEvent(This->display, This->window, Expose, xevent));
            drawPixmap(This);
            break;
        case ButtonRelease:
            makeWidget(This);
            break;
        default:
            break;
    }
}

static void
addXtEventHandler(PluginInstance *This)
{
     Display *dpy = (Display*) This->display;
     Window xwin = (Window) This->window;
     Widget xt_w = XtWindowToWidget(dpy, xwin);
     if (xt_w)
     {
         long event_mask = ExposureMask | ButtonReleaseMask | ButtonPressMask;
         XSelectInput(dpy, xwin, event_mask);
         XtAddEventHandler(xt_w, event_mask, False, (XtEventHandler)xt_event_handler, This);
     }
}

void
makePixmap(PluginInstance *This)
{
    createPixmap(This);
    drawPixmap(This);
    setCursor(This);
    addXtEventHandler(This);
}
