/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
/*
 * The following handles the loading, unloading and management of
 * various PCKS #11 modules
 */

#include <ctype.h>
#include "pkcs11.h"
#include "seccomon.h"
#include "secmod.h"
#include "secmodi.h"
   
#include "pk11pars.h" 

/*
 * for 3.4 we continue to use the old SECMODModule structure
 */
static SECMODModule *
pk11_CreateModule(char *library, char *moduleName, char *parameters, char *nss)
{
    SECMODModule *mod = SECMOD_NewModule();
    char *slotParams,*ciphers;
    if (mod == NULL) return NULL;

    mod->commonName = PORT_ArenaStrdup(mod->arena,moduleName ? moduleName : "");
    if (library) {
	mod->dllName = PORT_ArenaStrdup(mod->arena,library);
    }
    /* new field */
    if (parameters) {
	mod->libraryParams = PORT_ArenaStrdup(mod->arena,parameters);
    }
    mod->internal = pk11_argHasFlag("flags","internal",nss);
    mod->isFIPS = pk11_argHasFlag("flags","FIPS",nss);
    mod->isCritical = pk11_argHasFlag("flags","critical",nss);
    slotParams = pk11_argGetParamValue("slotParams",nss);
    mod->slotInfo = pk11_argParseSlotInfo(mod->arena,slotParams,
							&mod->slotInfoCount);
    if (slotParams) PORT_Free(slotParams);
    /* new field */
    mod->trustOrder = pk11_argReadLong("trustOrder",nss);
    /* new field */
    mod->cipherOrder = pk11_argReadLong("cipherOrder",nss);
    /* new field */
    mod->isModuleDB = pk11_argHasFlag("flags","moduleDB",nss);
    mod->moduleDBOnly = pk11_argHasFlag("flags","moduleDBOnly",nss);
    if (mod->moduleDBOnly) mod->isModuleDB = PR_TRUE;

    ciphers = pk11_argGetParamValue("ciphers",nss);
    pk11_argSetNewCipherFlags(&mod->ssl[0],ciphers);
    if (ciphers) PORT_Free(ciphers);

    return mod;
}
    

char **
pk11_getModuleSpecList(SECMODModule *module)
{
    SECMODModuleDBFunc func = (SECMODModuleDBFunc) module->moduleDBFunc;
    if (func) {
	return (*func)(SECMOD_MODULE_DB_FUNCTION_FIND,
		module->libraryParams,NULL);
    }
    return NULL;
}

pk11_freeModuleSpecList(char **moduleSpecList)
{
    char ** index;

    for(index = moduleSpecList; *index; index++) {
	PORT_Free(index);
    }
    PORT_Free(moduleSpecList);
}

SECStatus
PK11_LoadPKCS11Module(char *modulespec,SECMODModule *parent)
{
    char *library = NULL, *moduleName = NULL, *parameters = NULL, *nss= NULL;
    SECStatus status;
    SECMODModule *module = NULL;
    SECStatus rv;

    /* initialize the underlying module structures */
    SECMOD_Init();

    status = pk11_argParseModuleSpec(modulespec, &library, &moduleName, 
							&parameters, &nss);
    if (status != SECSuccess) {
	goto loser;
    }

    module = pk11_CreateModule(library, moduleName, parameters, nss);
    if (library) PORT_Free(library);
    if (moduleName) PORT_Free(moduleName);
    if (parameters) PORT_Free(parameters);
    if (nss) PORT_Free(nss);

    /* load it */
    rv = SECMOD_LoadModule(module);
    if (rv != SECSuccess) {
	goto loser;
    }

    if (module->isModuleDB) {
	char ** moduleSpecList;
	char **index;

	moduleSpecList = pk11_getModuleSpecList(module);

	for (index = moduleSpecList; index && *index; index++) {
	    rv = PK11_LoadPKCS11Module(*index,module);
	    if (rv != SECSuccess) break;
	}

	pk11_freeModuleSpecList(moduleSpecList);
    }

    if (rv != SECSuccess) {
	goto loser;
    }

    if (parent) {
    	module->parent = SECMOD_ReferenceModule(parent);
    }
    if (!module->moduleDBOnly) {
	SECMOD_AddModuleToList(module);
    } else {
	SECMOD_AddModuleToDBOnlyList(module);
    }
   
#ifdef notdef
    printf("-------------------------------------------------\n");
    printf("Module Name: %s\n",module->commonName);
    printf("Library Name: %s\n", module->dllName);
    printf("Module Parameters: %s\n", module->moduleParams);
    printf("Module type: %s\n",module->internal?"internal":"external");
    printf("FIPS module: %s\n", module->isFIPS ?"on": "off");
    printf("Trust Order: %d\n", module->trustOrder);
    printf("Cipher Order: %d\n", module->cipherOrder);
    printf("New Ciphers: 0x%08x,0x%08x\n",module->ssl[0], module->ssl[1]);
    printf("Can Load PKCS #11 module: %s\n", module->isModuleDB ? "True" : "False");
    if (module->slotInfoCount) {
	int i;
	printf("Slot Infos: %d\n", module->slotInfoCount);
	for (i=0; i < module->slotInfoCount; i++) {
	    printf("	++++++++++++++++++++++++++++++++++++++++++++\n");
	    printf("	Slot ID=0x%08x\n",module->slotInfo[i].slotID);
	    printf("	Slot Flags = 0x%08x\n",module->slotInfo[i].defaultFlags);
	    printf("	Slot PWarg = %d\n",module->slotInfo[i].askpw);
	    printf("	Slot timeout = %d\n",module->slotInfo[i].timeout);
	}
    }
#endif
    /* handle any additional work here */
    return SECSuccess;

loser:
    if (module) {
	PRBool critical = module->isCritical || (parent == NULL);
	SECMOD_DestroyModule(module);
	return critical ? SECFailure: SECSuccess;
    }
    return SECFailure;
}
