/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <stdio.h>

#include "nsXPCOM.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

#include "nsSpecialSystemDirectory.h" 

#include "nscore.h"
#include "nspr.h"

#include "nsSimpleNotifier.h"

/*********************************************
 SoftwareUpdate
*********************************************/
#include "nsISoftwareUpdate.h"
#include "nsSoftwareUpdateIIDs.h"

static nsISoftwareUpdate *softwareUpdate = NULL;
static NS_DEFINE_IID(kSoftwareUpdateCID, NS_SoftwareUpdate_CID);
/*********************************************/


static void
xpinstall_usage(int argc, char *argv[])
{
    fprintf(stderr, "Usage: %s [-f flags] [-a arguments] filename\n", argv[0]);
}

int
main(int argc, char **argv)
{
 
    for (int i = 1; i < argc; i++) 
    {
        if (argv[i][0] != '-')
            break;

        switch (argv[i][1]) 
        {
            case 'f':
            if (argv[i][2] == '\0' && i == argc) 
            {
                fputs("ERROR: missing path after -f\n", stderr);
                xpinstall_usage(argc, argv);
                return 1;
            }
            //foo = argv[i + 1];
            i++;
            break;

          default:
            fprintf(stderr, "unknown option %s\n", argv[i]);
            xpinstall_usage(argc, argv);
            return 1;
        }
    }



    nsCOMPtr<nsIServiceManager> servMan;
    NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, nsnull);
    nsCOMPtr<nsIComponentRegistrar> registrar = do_QueryInterface(servMan);
    if (!registrar) {
        NS_ASSERTION(0, "Null nsIComponentRegistrar");
        return rv;
    }
    registrar->AutoRegister(nsnull);


    nsresult rv = CallCreateInstance(kSoftwareUpdateCID, &softwareUpdate);

    if (NS_SUCCEEDED(rv))
    {
        nsSimpleNotifier *progress = new nsSimpleNotifier();

        nsFileSpec jarFile(argv[i]);
        nsFileURL jarFileURL(jarFile);
        
        softwareUpdate->InstallJar(  nsString( jarFileURL.GetAsString() ) ,
                                     nsString( nsNSPRPath(jarFile) ), 
                                     0x0000FFFF,
                                     progress);
    }

    return rv;
}
