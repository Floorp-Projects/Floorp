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

    static gint Kill(GtkWidget *widget, GtkWidget *event, gpointer data);

private:
    int         InitContext();
    GtkWidget   *DrawLogo();
    int         DrawCancelButton(GtkWidget *aLogoVBox);
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
