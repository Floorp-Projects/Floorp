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

#ifndef _NS_XICONTEXT_H_
#define _NS_XICONTEXT_H_

#include <gtk/gtk.h>

#include "nsLicenseDlg.h"
#include "nsWelcomeDlg.h"
#include "nsSetupTypeDlg.h"
#include "nsComponentsDlg.h"
#include "nsInstallDlg.h"
#include "nsXIOptions.h"

class nsXInstaller;

class nsXIContext
{
public:
    nsXIContext();
    ~nsXIContext();

    nsXInstaller        *me;

/*-------------------------------------------------------------------*
 *   Dialog Contexts
 *-------------------------------------------------------------------*/
    nsLicenseDlg        *ldlg;
    nsWelcomeDlg        *wdlg;
    nsSetupTypeDlg      *sdlg;
    nsComponentsDlg     *cdlg;
    nsInstallDlg        *idlg;

    nsXIOptions         *opt;

/*-------------------------------------------------------------------*
 *   Global Widgets
 *-------------------------------------------------------------------*/
    GtkWidget           *window;    /* unique canvas for dialogs */
    GtkWidget           *back;      /* back button */
    GtkWidget           *next;      /* next button */
    GtkWidget           *nextLabel;     /* "Next" label */
    GtkWidget           *backLabel;     /* "Back" label */
    GtkWidget           *acceptLabel;   /* "Accept" label */
    GtkWidget           *declineLabel;  /* "Decline" label */
    GtkWidget           *installLabel;  /* "Install" label */
    GtkWidget           *logo;      /* branding icon: an xpm image */
    GtkWidget           *mainbox;   /* vbox holding all except logo */
    GtkWidget           *canvas;    /* vbox for mutable dlgs (no nav btns) */
    GtkWidget           *notebook;  /* notebook whose pages are dlgs */

    int                 backID;     /* signal handler id for back btn */
    int                 nextID;     /* signal handler id for next btn */
    int                 bMoving;    /* when moving between dlgs signals are
                                       emitted twice; this notes the state */
/*-------------------------------------------------------------------*
 *   Utilities
 *-------------------------------------------------------------------*/
    char    *itoa(int n);
};

#endif /* _NS_XICONTEXT_H_ */
