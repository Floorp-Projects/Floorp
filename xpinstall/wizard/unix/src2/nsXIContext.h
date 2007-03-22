/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Samir Gehani <sgehani@netscape.com>
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

#ifndef _NS_XICONTEXT_H_
#define _NS_XICONTEXT_H_

#include "nsLicenseDlg.h"
#include "nsWelcomeDlg.h"
#include "nsSetupTypeDlg.h"
#include "nsComponentsDlg.h"
#include "nsInstallDlg.h"
#include "nsXIOptions.h"
#include "nsINIParser.h"

#define RES_FILE "installer"
#define RES_SECT "String Resources"

class nsXInstaller;

typedef struct _kvpair
{
    char *key;
    char *val;

    struct _kvpair *next;
} kvpair;

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
    GtkWidget           *cancel;    /* cancel button */
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
    int                 cancelID;   /* signal handler id for cancel btn */
    int                 bMoving;    /* when moving between dlgs signals are
                                       emitted twice; this notes the state */
    int                 bDone;      /* engine thread sets boolean when done
                                       so that ui/main thread can exit */

/*-------------------------------------------------------------------*
 *   Utilities
 *-------------------------------------------------------------------*/
    int     LoadResources();        /* load string resources */
    int     ReleaseResources();     /* release alloc'd resource strings */
    char    *Res(char *aKey);       /* get string resource for key; NULL==err */

private:
    kvpair  *reslist;               /* string resource linked list */
};

#endif /* _NS_XICONTEXT_H_ */
