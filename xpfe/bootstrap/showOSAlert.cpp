/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Don Bragg <dbragg@netscape.com>
 */

#include  <stdio.h>
#include "nscore.h"

//defines and includes for previous installation cleanup process
#if defined (XP_WIN)
#include <windows.h>
#elif defined (XP_MAC)
#include <Dialogs.h>
#include <TextUtils.h>
#elif defined (MOZ_WIDGET_GTK)
#include <gtk/gtk.h>
#elif defined (XP_OS2)
#include <os2.h>
#endif

extern "C" void ShowOSAlert(char* aMessage);

#if defined (MOZ_WIDGET_GTK)

static int sbAlertDone = 0;

/**
 * ns_gtk_alert_OK_callback
 *
 * Private callback function for the OK button on the alert.
 *
 * @param aWidget   the button widget
 * @param aData     the alert dialog passed in for destruction
 */
void
ns_gtk_alert_OK_callback(GtkWidget *aWidget, gpointer aData)
{
    GtkWidget *alertDlg = (GtkWidget *) aData;

    if (!alertDlg)
        return;

    gtk_widget_destroy(alertDlg);
    sbAlertDone = 1;
}

/**
 * NS_gtk_alert
 *
 * Displays an alert with a message and an OK button in a modal 
 * dialog that is centered on the screen.  Assumes gtk_main has 
 * been called or explicit event queue flushing is being performed.
 *
 * @param aMessage   [required] the message to display in the alert
 * @param aTitle     [optional] the alert dialog title;
 *                              passing NULL defaults the title to ""
 * @param aOKBtnText [optional] the text on the OK button;
 *                              parametrized for i18n purposes;
 *                              passing NULL defaults the button text to "OK"
 * @return int error code       returns 0 for SUCCESS;
 *                              returns -1 for param error;
 *                              returns -2 for widget creation error
 */
int
NS_gtk_alert(char *aMessage, char *aTitle, char *aOKBtnText)
{
#ifdef DEBUG_dbragg
    printf ("\n*** Now inside NS_gtk_alert *** \n");
#endif 

    GtkWidget *alertDlg = NULL;
    GtkWidget *okBtn = NULL;
    GtkWidget *msgLabel = NULL;
    GtkWidget *packerLbl = NULL;
    GtkWidget *packerBtn = NULL;
    char *okBtnText = aOKBtnText;
    char *title = aTitle;

    if (!aMessage)
        return -1;
    if (!aOKBtnText)
        okBtnText = "OK";
    if (!aTitle)
        title = " ";

#ifdef DEBUG_dbragg
    printf("title is: %s\n", title);
#endif

    alertDlg = gtk_dialog_new();
    msgLabel = gtk_label_new(aMessage);
    okBtn = gtk_button_new_with_label(okBtnText);
    packerLbl = gtk_packer_new();
    packerBtn = gtk_packer_new();
    sbAlertDone = 0;

    if (alertDlg && msgLabel && okBtn && packerBtn && packerLbl)
    {
        // pack widgets in dialog 
        gtk_packer_set_default_border_width(GTK_PACKER(packerLbl), 20);
        gtk_packer_add_defaults(GTK_PACKER(packerLbl), msgLabel, 
            GTK_SIDE_BOTTOM, GTK_ANCHOR_CENTER, GTK_FILL_X);
        gtk_packer_set_default_border_width(GTK_PACKER(packerBtn), 0);
        gtk_packer_add_defaults(GTK_PACKER(packerBtn), okBtn, 
            GTK_SIDE_BOTTOM, GTK_ANCHOR_CENTER, GTK_FILL_Y);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(alertDlg)->vbox), 
            packerLbl);
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(alertDlg)->action_area),
            packerBtn);

        gtk_window_set_modal(GTK_WINDOW(alertDlg), TRUE);
        gtk_window_set_title(GTK_WINDOW(alertDlg), title);
        gtk_window_set_position(GTK_WINDOW(alertDlg), GTK_WIN_POS_CENTER);

        // register callback for OK button
        gtk_signal_connect(GTK_OBJECT(okBtn), "clicked", 
            GTK_SIGNAL_FUNC(ns_gtk_alert_OK_callback), alertDlg);
        GTK_WIDGET_SET_FLAGS(okBtn, GTK_CAN_DEFAULT);
        gtk_widget_grab_default(okBtn);

        // show dialog
        gtk_widget_show(msgLabel);
        gtk_widget_show(packerLbl);
        gtk_widget_show(okBtn);
        gtk_widget_show(packerBtn);
        gtk_widget_show(alertDlg);
    }
    else
    {
        return -2;
    }

    while (!sbAlertDone)
    {
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    return 0;
}
#endif //MOZ_WIDGET_GTK


void ShowOSAlert(char* aMessage)
{
#ifdef DEBUG_dbragg
printf("\n****Inside ShowOSAlert ***\n");	
#endif 

#if defined (XP_WIN)
    MessageBox(NULL, aMessage, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND );
#elif (XP_MAC)
    short buttonClicked;
    StandardAlert(kAlertStopAlert, c2pstr(aMessage), nil, nil, &buttonClicked);
#elif defined (MOZ_WIDGET_GTK)
    NS_gtk_alert(aMessage, NULL, "OK");
#elif defined (XP_OS2)
    HAB hab = WinInitialize(0);
    HMQ hmq = WinCreateMsgQueue(hmq,0);
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, aMessage, "", 0, MB_OK);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
#else
    fprintf(stdout, "%s\n", aMessage);
#endif

}

