/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#include "XIErrors.h"
#include "XIDefines.h"

static GtkWidget *sErrDlg;

static char *ERRORS[] = {
        "___ACCOUNT_FOR_ZERO_INDEX___",

        "Out of memory", 
        "Invalid param",
        "Invalid member variable",
        "Invalid pointer",
        "Parse key has no value",
        "Failed to read readme",
        "Failed to read license",
        "Out of bounds in Component/Setup Type list",
        "Mismatched ref counts",
        "No components in the INI file",
        "No setup types in the INI file",
        "URL at this index already exists",
        "Couldn't create the directory",
        "FTP URL malformed",
        "Download failed",
        "Extraction of XPCOM failed",
        "Failed to fork a process",
        "Couldn't open xpistub library",
        "Couldn't get symbol in library",
        "A xpistub call failed",
        "A .xpi failed to install",
        "Copy of a .xpi failed", 
        "Destination directory doesn't exist",
        "Can't make destination directory",

        "___PREVENT_FALLING_OF_END___"
};

/*------------------------------------------------------------------*
 *   Default Error Handler
 *------------------------------------------------------------------*/
int 
ErrorHandler(int aErr)
{
    GtkWidget *okButton, *label;
    char msg[256];
    int resNum;

    resNum = -1 * (aErr % 100);
    
    if (aErr < FATAL_ERR_THRESHOLD)
        sprintf(msg, FATAL_ERROR, aErr, ERRORS[resNum]); 
    else if (aErr < GENERAL_ERR_THRESHOLD)
        sprintf(msg, ERROR, aErr, ERRORS[resNum]);
    else 
    {
        printf(msg, WARNING, aErr, ERRORS[resNum]);
        return aErr;
    }
    
    sErrDlg = gtk_dialog_new();
    okButton = gtk_button_new_with_label(OK_LABEL);
    label = gtk_label_new(msg);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sErrDlg)->action_area), 
                      okButton);
    gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
                       GTK_SIGNAL_FUNC(ErrDlgOK), (void*)aErr);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(sErrDlg)->vbox), label);
    
    gtk_widget_show_all(sErrDlg);

    return aErr;
}

void
ErrDlgOK(GtkWidget *aWidget, gpointer aData)
{
    int err = (int) aData;
    
    gtk_widget_destroy(sErrDlg);
    if (err < FATAL_ERR_THRESHOLD)
        exit(err);
}
