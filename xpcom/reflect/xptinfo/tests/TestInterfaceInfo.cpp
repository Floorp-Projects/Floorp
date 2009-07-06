/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* Some simple smoke tests of the typelib loader. */

#include "nscore.h"

#include "nsISupports.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"
#include "nsServiceManagerUtils.h"

#include <stdio.h>

// This file expects the nsInterfaceInfoManager to be able to discover
// .xpt files corresponding to those in xpcom/idl.  Currently this
// means setting XPTDIR in the environment to some directory
// containing these files.

int main (int argc, char **argv) {
    int i;
    nsIID *iid1, *iid2, *iid3;
    char *name1, *name2, *name3;
    nsIInterfaceInfo *info2, *info3, *info4, *info5;

    nsCOMPtr<nsIInterfaceInfoManager> iim
        (do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));

    fprintf(stderr, "\ngetting iid for 'nsISupports'\n");
    iim->GetIIDForName("nsISupports", &iid1);
    iim->GetNameForIID(iid1, &name1);
    fprintf(stderr, "%s iid %s\n", name1, iid1->ToString());

    fprintf(stderr, "\ngetting iid for 'nsIInputStream'\n");
    iim->GetIIDForName("nsIInputStream", &iid2);
    iim->GetNameForIID(iid2, &name2);
    fprintf(stderr, "%s iid %s\n", name2, iid2->ToString());

    fprintf(stderr, "iid: %s, name: %s\n", iid1->ToString(), name1);
    fprintf(stderr, "iid: %s, name: %s\n", iid2->ToString(), name2);

    fprintf(stderr, "\ngetting info for iid2 from above\n");
    iim->GetInfoForIID(iid2, &info2);
#ifdef DEBUG
//    ((nsInterfaceInfo *)info2)->print(stderr);
#endif

    fprintf(stderr, "\ngetting iid for 'nsIInputStream'\n");
    iim->GetIIDForName("nsIInputStream", &iid3);
    iim->GetNameForIID(iid3, &name3);
    fprintf(stderr, "%s iid %s\n", name3, iid2->ToString());
    iim->GetInfoForIID(iid3, &info3);
#ifdef DEBUG
//    ((nsInterfaceInfo *)info3)->print(stderr);
#endif

    fprintf(stderr, "\ngetting info for name 'nsIBidirectionalEnumerator'\n");
    iim->GetInfoForName("nsIBidirectionalEnumerator", &info4);
#ifdef DEBUG
//    ((nsInterfaceInfo *)info4)->print(stderr);
#endif

    fprintf(stderr, "\nparams work?\n");
    fprintf(stderr, "\ngetting info for name 'nsIServiceManager'\n");
    iim->GetInfoForName("nsIComponentManager", &info5);
#ifdef DEBUG
//    ((nsInterfaceInfo *)info5)->print(stderr);
#endif

    // XXX: nsIServiceManager is no more; what do we test with?
    if (info5 == NULL) {
        fprintf(stderr, "\nNo nsIComponentManager; cannot continue.\n");
        return 1;
    }

    uint16 methodcount;
    info5->GetMethodCount(&methodcount);
    const nsXPTMethodInfo *mi;
    for (i = 0; i < methodcount; i++) {
        info5->GetMethodInfo(i, &mi);
        fprintf(stderr, "method %d, name %s\n", i, mi->GetName());
    }

    // 4 is getServiceByContractID, which has juicy params.
    info5->GetMethodInfo(6, &mi);

    const nsXPTParamInfo& param2 = mi->GetParam(1);
    // should be IID for the service
    nsIID *nsISL;
    info5->GetIIDForParam(6, &param2, &nsISL);
    fprintf(stderr, "iid assoc'd with param 1 of method 6 - createInstanceByContractID - %s\n", nsISL->ToString());
    // if we look up the name?
    char *nsISLname;
    iim->GetNameForIID(nsISL, &nsISLname);
    fprintf(stderr, "which is called %s\n", nsISLname);

    fprintf(stderr, "\nNow check the last param\n");
    const nsXPTParamInfo& param3 = mi->GetParam(3);

    if (param3.GetType().TagPart() != nsXPTType::T_INTERFACE_IS) {
        fprintf(stderr, "Param 3 is not type interface is\n");
        // Not returning an error, because this could legitamately change
    }
    // lets see what arg this refers to
    uint8 argnum;
    info5->GetInterfaceIsArgNumberForParam(6, &param3, &argnum);
    fprintf(stderr, "param 3 referrs to param %d of method 6 - createInstanceByContractID\n", (PRUint32)argnum);
    // Get the type of the parameter referred to
    const nsXPTParamInfo& arg_param = mi->GetParam(argnum);
    const nsXPTType& arg_type = arg_param.GetType();
    // Check to make sure it refers to the proper param
    if(!arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_IID) {
        fprintf(stderr, "Param 3 of method 6 refers to a non IID parameter\n"); 
        // Not returning an error, because this could legitamately change
    }


    return 0;
}    

