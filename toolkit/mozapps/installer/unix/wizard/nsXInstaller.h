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

#ifndef _NS_XINSTALLER_H_
#define _NS_XINSTALLER_H_

#include "XIDefines.h"
#include "XIErrors.h"

#include "nsINIParser.h"
#include "nsLicenseDlg.h"
#include "nsXIContext.h"

extern nsXIContext *gCtx; 

class nsXInstaller
{
public:
    nsXInstaller();
    ~nsXInstaller();

    int ParseArgs(int aArgc, char **aArgv);
    int ParseConfig();
    int RunWizard(int argc, char **argv);
    int ParseGeneral(nsINIParser *aParser);

    static gint Kill(GtkWidget *widget, GdkEvent *event, gpointer data);

private:
    static gboolean HandleKeyPress(GtkWidget *widget, GdkEventKey *event,
                                   gpointer data);

    int         InitContext();
    void        SetupBoxes();
    int         DrawNavButtons();
};

int     main(int argc, char **argv);
int     ErrorHandler(int aErr, const char *aErrMsg=NULL);
void    ErrDlgOK(GtkWidget *aWidget, gpointer aData);
int     IsErrFatal(int aErr);

#define CONFIG "config"

#if defined(DUMP)
#undef DUMP
#endif
#if defined(DEBUG_sgehani) || defined(DEBUG_druidd) || defined(DEBUG_root)
#define DUMP(_msg) printf("%s %d: %s \n", __FILE__, __LINE__, _msg);
#else
#define DUMP(_msg)
#endif


#endif /*_NS_XINSTALLER_H_ */
