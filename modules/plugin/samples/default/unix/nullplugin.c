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
#include "npapi.h"
#include "nullplugin.h"

/* Global data */
static MimeTypeElement *head = NULL;

/* this function is used to clear the mime type cache list 
   whenever the dialog box is closed. We need to clear the
   list in order to have the dialog box pop up again when
   the page is reload. (it's because there is no puzzle
   icon in unix to let the people actively forward to netscape
   page)  */
static void 
clearList(MimeTypeElement **typelist) 
{
    MimeTypeElement *ele;
    MimeTypeElement *ele2;

    if (typelist == NULL)
        return;

    /* follow the head to free the list */
    ele = *typelist;
    while (ele != NULL) {
        ele2 = ele->next;
        if (ele->value != NULL)
            NPN_MemFree(ele->value);
        NPN_MemFree(ele);
        ele = ele2;
    }
    *typelist = ele; 
    return;
}

/* callback function for the OK button */
static void 
DialogOKClicked (GtkButton *button, gpointer data)
{
    PluginInstance* This = (PluginInstance*) data;
    char *url;

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
        if (address == NULL)
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
    gtk_widget_destroy (This->dialogBox); 
    clearList(&head);
}

/* the call back function for cancel button */
static void 
DialogCancelClicked (GtkButton *button, gpointer data) 
{
    gtk_widget_destroy (GTK_WIDGET(data));
    clearList(&head);
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
    return(strcmp(t1, t2) == 0);
}

static gboolean 
isExist(MimeTypeElement **typelist, NPMIMEType type)
{
    MimeTypeElement *ele;

    if (typelist == NULL) return FALSE;

    ele = *typelist;
    while (ele != NULL) {
        if (isEqual(ele->value, type))
            return TRUE;
        ele = ele->next;
    }
    return FALSE;
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
addToList(MimeTypeElement **typelist, NPMIMEType type)
{
    MimeTypeElement *ele;

    if (!typelist) return(FALSE);

    if (isExist(typelist, type)) return(FALSE);

    ele = (MimeTypeElement *) NPN_MemAlloc(sizeof(MimeTypeElement));
    ele->value = dupMimeType(type);
    ele->next = *typelist;
    *typelist = ele;
    return(TRUE);
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

    if (!This) return;

    dialogWindow = gtk_dialog_new();
    This->dialogBox = dialogWindow;
    gtk_window_set_title(GTK_WINDOW(dialogWindow), PLUGIN_NAME);
    /* gtk_window_set_position(GTK_WINDOW(dialogWindow), GTK_WIN_POS_CENTER); */
    gtk_window_set_modal(GTK_WINDOW(dialogWindow), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(dialogWindow), 0);

    PR_snprintf(message, sizeof(message) - 1, MESSAGE, This->type);
    dialogMessage = AddWidget(gtk_label_new (message), 
                   GTK_DIALOG(dialogWindow)->vbox);

    okButton= AddWidget(gtk_button_new_with_label (OK_BUTTON), 
                   GTK_DIALOG(dialogWindow)->action_area);

    cancelButton= AddWidget(gtk_button_new_with_label (CANCEL_BUTTON), 
                   GTK_DIALOG(dialogWindow)->action_area);

    gtk_signal_connect (GTK_OBJECT(okButton),  "clicked",
                        GTK_SIGNAL_FUNC(DialogOKClicked), This);

    gtk_signal_connect (GTK_OBJECT(cancelButton),  "clicked",
                        GTK_SIGNAL_FUNC(DialogCancelClicked), dialogWindow);


    /* need to check whether we already pop up a dialog box for previous
       minetype in the same web page. It's require otherwise there will
       be 2 dialog boxes pop if there are 2 plugin in the same web page */
    if (addToList(&head, This->type)) { 
        gtk_widget_show_all(dialogWindow);
    } 
}

