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
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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

#include "modutil.h"
#include "secmodti.h"
#include "pk11func.h"

extern PK11DefaultArrayEntry PK11_DefaultArray[];
extern int num_pk11_default_mechanisms;
extern SECStatus PK11_UpdateSlotAttribute(PK11SlotInfo*, PK11DefaultArrayEntry*,
	PRBool);

/*************************************************************************
 *
 * F i p s M o d e
 * If arg=="true", enable FIPS mode on the internal module.  If arg=="false",
 * disable FIPS mode on the internal module.
 */
Error
FipsMode(char *arg)
{

	char *internal_name;

	if(!PORT_Strcasecmp(arg, "true")) {
		if(!PK11_IsFIPS()) {
			internal_name = PR_smprintf("%s",
				SECMOD_GetInternalModule()->commonName);
			if(SECMOD_DeleteInternalModule(internal_name) != SECSuccess) {
				PR_smprintf_free(internal_name);
				PR_fprintf(PR_STDERR, errStrings[FIPS_SWITCH_FAILED_ERR]);
				return FIPS_SWITCH_FAILED_ERR;
			}
			PR_smprintf_free(internal_name);
			PR_fprintf(PR_STDOUT, msgStrings[FIPS_ENABLED_MSG]);
		} else {
			PR_fprintf(PR_STDERR, errStrings[FIPS_ALREADY_ON_ERR]);
			return FIPS_ALREADY_ON_ERR;
		}
	} else if(!PORT_Strcasecmp(arg, "false")) {
		if(PK11_IsFIPS()) {
			internal_name = PR_smprintf("%s",
				SECMOD_GetInternalModule()->commonName);
			if(SECMOD_DeleteInternalModule(internal_name) != SECSuccess) {
				PR_smprintf_free(internal_name);
				PR_fprintf(PR_STDERR, errStrings[FIPS_SWITCH_FAILED_ERR]);
				return FIPS_SWITCH_FAILED_ERR;
			}
			PR_smprintf_free(internal_name);
			PR_fprintf(PR_STDOUT, msgStrings[FIPS_DISABLED_MSG]);
		} else {
			PR_fprintf(PR_STDERR, errStrings[FIPS_ALREADY_OFF_ERR]);
			return FIPS_ALREADY_OFF_ERR;
		}
	} else {
		PR_fprintf(PR_STDERR, errStrings[INVALID_FIPS_ARG]);
		return INVALID_FIPS_ARG;
	}

	return SUCCESS;
}

/************************************************************************
 * Cipher and Mechanism name-bitmask translation tables
 */

typedef struct {
	char *name;
	unsigned long mask;
} MaskString;

static MaskString mechanismStrings[] = {
	{"RSA", PUBLIC_MECH_RSA_FLAG},
	{"DSA", PUBLIC_MECH_DSA_FLAG},
	{"RC2", PUBLIC_MECH_RC2_FLAG},
	{"RC4", PUBLIC_MECH_RC4_FLAG},
	{"RC5", PUBLIC_MECH_RC5_FLAG},
	{"DES", PUBLIC_MECH_DES_FLAG},
	{"DH", PUBLIC_MECH_DH_FLAG},
	{"FORTEZZA", PUBLIC_MECH_FORTEZZA_FLAG},
	{"SHA1", PUBLIC_MECH_SHA1_FLAG},
	{"MD5", PUBLIC_MECH_MD5_FLAG},
	{"MD2", PUBLIC_MECH_MD2_FLAG},
	{"SSL", PUBLIC_MECH_SSL_FLAG},
	{"TLS", PUBLIC_MECH_TLS_FLAG},
	{"RANDOM", PUBLIC_MECH_RANDOM_FLAG},
	{"FRIENDLY", PUBLIC_MECH_FRIENDLY_FLAG}
};
static int numMechanismStrings =
	sizeof(mechanismStrings) / sizeof(mechanismStrings[0]);

static MaskString cipherStrings[] = {
	{"FORTEZZA", PUBLIC_CIPHER_FORTEZZA_FLAG}
};
static int numCipherStrings =
	sizeof(cipherStrings) / sizeof(cipherStrings[0]);

/* Maximum length of a colon-separated list of all the strings in an 
 * array. */
#define MAX_STRING_LIST_LEN 240	/* or less */

/************************************************************************
 * 
 * g e t F l a g s F r o m S t r i n g
 *
 * Parses a mechanism list passed on the command line and converts it
 * to an unsigned long bitmask.
 * string is a colon-separated string of constants
 * array is an array of MaskStrings.
 * elements is the number of elements in array.
 */
static unsigned long
getFlagsFromString(char *string, MaskString array[], int elements)
{
	unsigned long ret = 0;
	short i = 0;
	char *cp;
	char *buf;
	char *end;

	if(!string || !string[0]) {
		return ret;
	}

	/* Make a temporary copy of the string */
	buf = PR_Malloc(strlen(string)+1);
	if(!buf) {
		out_of_memory();
	}
	strcpy(buf, string);

	/* Look at each element of the list passed in */
	for(cp=buf; cp && *cp; cp = (end ? end+1 : NULL) ) {
		/* Look at the string up to the next colon */
		end = strchr(cp, ':');
		if(end) {
			*end = '\0';
		}

		/* Find which element this is */
		for(i=0; i < elements; i++) {
			if( !PORT_Strcasecmp(cp, array[i].name) ) {
				break;
			}
		}
		if(i == elements) {
			/* Skip a bogus string, but print a warning message */
			PR_fprintf(PR_STDERR, errStrings[INVALID_CONSTANT_ERR], cp);
			continue;
		}
		ret |= array[i].mask;
	}

	PR_Free(buf);
	return ret;
}

/**********************************************************************
 *
 * g e t S t r i n g F r o m F l a g s
 * 
 * The return string's memory is owned by this function.  Copy it
 * if you need it permanently or you want to change it.
 */
static char *
getStringFromFlags(unsigned long flags, MaskString array[], int elements)
{
	static char buf[MAX_STRING_LIST_LEN];
	int i;
	int count=0;

	buf[0] = '\0';
	for(i=0; i<elements; i++) {
		if( flags & array[i].mask ) {
			++count;
			if(count!=1) {
				strcat(buf, ":");
			}
			strcat(buf, array[i].name);
		}
	}
	return buf;
}

/**********************************************************************
 *
 * A d d M o d u l e
 *
 * Add the named module, with the given library file, ciphers, and
 * default mechanism flags
 */
Error
AddModule(char *moduleName, char *libFile, char *cipherString,
	char *mechanismString)
{
	unsigned long ciphers;
	unsigned long mechanisms;
	SECStatus status;

	mechanisms =
		getFlagsFromString(mechanismString, mechanismStrings,
		  numMechanismStrings);
	ciphers =
		getFlagsFromString(cipherString, cipherStrings, numCipherStrings);

	status =
		SECMOD_AddNewModule(moduleName, libFile,
		  SECMOD_PubMechFlagstoInternal(mechanisms),
		  SECMOD_PubCipherFlagstoInternal(ciphers) );

	if(status != SECSuccess) {
		PR_fprintf(PR_STDERR, errStrings[ADD_MODULE_FAILED_ERR], moduleName);
		return ADD_MODULE_FAILED_ERR;
	} else {
		PR_fprintf(PR_STDOUT, msgStrings[ADD_MODULE_SUCCESS_MSG], moduleName);
		return SUCCESS;
	}
}

/***********************************************************************
 *
 * D e l e t e M o d u l e
 *
 * Deletes the named module from the database.
 */
Error
DeleteModule(char *moduleName)
{
	SECStatus status;
	int type;
	
	status = SECMOD_DeleteModule(moduleName, &type);

	if(status != SECSuccess) {
		if(type == SECMOD_FIPS || type == SECMOD_INTERNAL) {
			PR_fprintf(PR_STDERR, errStrings[DELETE_INTERNAL_ERR]);
			return DELETE_INTERNAL_ERR;
		} else {
			PR_fprintf(PR_STDERR, errStrings[DELETE_FAILED_ERR], moduleName);
			return DELETE_FAILED_ERR;
		}
	}

	PR_fprintf(PR_STDOUT, msgStrings[DELETE_SUCCESS_MSG], moduleName);
	return SUCCESS;
}

/************************************************************************
 *
 * R a w L i s t M o d u l e s
 *
 * Lists all the modules in the database, along with their slots and tokens.
 */
Error
RawListModule(char *modulespec)
{
	SECMODModule *module;
	char **moduleSpecList;
	SECStatus rv;

	module = SECMOD_LoadModule(modulespec,NULL,PR_FALSE);
	if (module == NULL) {
	    /* handle error */
	    return NO_SUCH_MODULE_ERR;
	}

	moduleSpecList = SECMOD_GetModuleSpecList(module);

	for ( ;*moduleSpecList; moduleSpecList++) {
		printf("%s\n",*moduleSpecList);
	}

	return SUCCESS;
}

RawAddModule(char *dbmodulespec, char *modulespec)
{
    SECMODModule *module;
    SECMODModule *dbmodule;


    dbmodule = SECMOD_LoadModule(dbmodulespec,NULL,PR_TRUE);
    if (dbmodule == NULL) {
	 /* handle error */
	return NO_SUCH_MODULE_ERR;
    }

    module = SECMOD_LoadModule(modulespec,dbmodule,PR_FALSE);
    if (module == NULL) {
	 /* handle error */
	return NO_SUCH_MODULE_ERR;
    }

    if( SECMOD_UpdateModule(module) != SECSuccess ) {
	PR_fprintf(PR_STDERR, errStrings[UPDATE_MOD_FAILED_ERR], modulespec);
	return UPDATE_MOD_FAILED_ERR;
    }
    return SUCCESS;
}

/************************************************************************
 *
 * L i s t M o d u l e s
 *
 * Lists all the modules in the database, along with their slots and tokens.
 */
Error
ListModules()
{
	SECMODListLock *lock;
	SECMODModuleList *list;
	SECMODModuleList *mlp;
	Error ret=UNSPECIFIED_ERR;
	int count = 0, i;

	lock = SECMOD_GetDefaultModuleListLock();
	if(!lock) {
		PR_fprintf(PR_STDERR, errStrings[NO_LIST_LOCK_ERR]);
		return NO_LIST_LOCK_ERR;
	}

	SECMOD_GetReadLock(lock);

	list = SECMOD_GetDefaultModuleList();
	if(!list) {
		PR_fprintf(PR_STDERR, errStrings[NO_MODULE_LIST_ERR]);
		ret = NO_MODULE_LIST_ERR;
		goto loser;
	}

	PR_fprintf(PR_STDOUT,
		"\nListing of PKCS #11 Modules\n"
		"-----------------------------------------------------------\n");
	
	for(mlp=list; mlp != NULL; mlp = mlp->next) {
		++count;
		if(count!=1) {
			PR_fprintf(PR_STDOUT, "\n");
		}

		PR_fprintf(PR_STDOUT, "%3d. %s\n", count, mlp->module->commonName);

		if(mlp->module->dllName) {
			PR_fprintf(PR_STDOUT, "\tlibrary name: %s\n", mlp->module->dllName);
		}

		if(mlp->module->slotCount == 0) {
			PR_fprintf(PR_STDOUT,
				"\t slots: There are no slots attached to this module\n");
		} else {
			PR_fprintf(PR_STDOUT,
				"\t slots: %d slot%s attached\n", mlp->module->slotCount,
				(mlp->module->slotCount==1 ? "" : "s") );
		}

		if(mlp->module->loaded == 0) {
			PR_fprintf(PR_STDOUT, "\tstatus: Not loaded\n");
		} else {
			PR_fprintf(PR_STDOUT, "\tstatus: loaded\n");
		}

		/* Print slot and token names */
		for (i = 0; i < mlp->module->slotCount; i++) {
			PK11SlotInfo *slot = mlp->module->slots[i];

			PR_fprintf(PR_STDOUT, "\n");
			PR_fprintf(PR_STDOUT, "\t slot: %s\n", PK11_GetSlotName(slot));
			PR_fprintf(PR_STDOUT, "\ttoken: %s\n", PK11_GetTokenName(slot));
		}
    }

	PR_fprintf(PR_STDOUT,
		"-----------------------------------------------------------\n");

	ret = SUCCESS;

loser:
	SECMOD_ReleaseReadLock(lock);
	return ret;
}

/* Strings describing PK11DisableReasons */
static char *disableReasonStr[] = {
	"no reason",
	"user disabled",
	"could not initialize token",
	"could not verify token",
	"token not present"
};
static int numDisableReasonStr =
	sizeof(disableReasonStr) / sizeof(disableReasonStr[0]);

/***********************************************************************
 *
 * L i s t M o d u l e
 *
 * Lists detailed information about the named module.
 */
Error
ListModule(char *moduleName)
{
	SECMODModule *module;
	PK11SlotInfo *slot;
	int slotnum;
	CK_INFO modinfo;
	CK_SLOT_INFO slotinfo;
	CK_TOKEN_INFO tokeninfo;
	char *ciphers, *mechanisms;
	PK11DisableReasons reason;

	if(!moduleName) {
		return SUCCESS;
	}

	module = SECMOD_FindModule(moduleName);
	if(!module) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_MODULE_ERR], moduleName);
		return NO_SUCH_MODULE_ERR;
	}

	if(PK11_GetModInfo(module, &modinfo) != SECSuccess) {
		PR_fprintf(PR_STDERR, errStrings[MOD_INFO_ERR], moduleName);
		return MOD_INFO_ERR;
	}

	/* Module info */
	PR_fprintf(PR_STDOUT, 
		"\n-----------------------------------------------------------\n");
	PR_fprintf(PR_STDOUT, "Name: %s\n", module->commonName);
	if(module->internal || !module->dllName) {
		PR_fprintf(PR_STDOUT, "Library file: **Internal ONLY module**\n");
	} else {
		PR_fprintf(PR_STDOUT, "Library file: %s\n", module->dllName);
	}

	PR_fprintf(PR_STDOUT, "Manufacturer: %.32s\n", modinfo.manufacturerID);
	PR_fprintf(PR_STDOUT, "Description: %.32s\n", modinfo.libraryDescription);
	PR_fprintf(PR_STDOUT, "PKCS #11 Version %d.%d\n",
		modinfo.cryptokiVersion.major, modinfo.cryptokiVersion.minor);
	PR_fprintf(PR_STDOUT, "Library Version: %d.%d\n",
		modinfo.libraryVersion.major, modinfo.libraryVersion.minor);

	/* Get cipher and mechanism flags */
	ciphers = getStringFromFlags(module->ssl[0], cipherStrings,
	  numCipherStrings);
	if(ciphers[0] == '\0') {
		ciphers = "None";
	}
	PR_fprintf(PR_STDOUT, "Cipher Enable Flags: %s\n", ciphers);
	mechanisms = NULL;
	if(module->slotCount > 0) {
		mechanisms = getStringFromFlags(module->slots[0]->defaultFlags,
			mechanismStrings, numMechanismStrings);
	}
	if(mechanisms[0] =='\0') {
		mechanisms = "None";
	}
	PR_fprintf(PR_STDOUT, "Default Mechanism Flags: %s\n", mechanisms);

#define PAD "  "

	/* Loop over each slot */
	for(slotnum=0; slotnum < module->slotCount; slotnum++) {
		slot = module->slots[slotnum];
		if(PK11_GetSlotInfo(slot, &slotinfo) != SECSuccess) {
			PR_fprintf(PR_STDERR, errStrings[SLOT_INFO_ERR],
				PK11_GetSlotName(slot));
			return SLOT_INFO_ERR;
		}
		if(PK11_GetTokenInfo(slot, &tokeninfo) != SECSuccess) {
			PR_fprintf(PR_STDERR, errStrings[TOKEN_INFO_ERR],
			  slot->token_name);
			return TOKEN_INFO_ERR;
		}

		/* Slot Info */
		PR_fprintf(PR_STDOUT, "\n"PAD"Slot: %s\n", PK11_GetSlotName(slot));
		mechanisms = getStringFromFlags(slot->defaultFlags,
			mechanismStrings, numMechanismStrings);
		if(mechanisms[0] =='\0') {
		     mechanisms = "None";
		}
		PR_fprintf(PR_STDOUT, PAD"Slot Mechanism Flags: %s\n", mechanisms);
		PR_fprintf(PR_STDOUT, PAD"Manufacturer: %.32s\n",
			slotinfo.manufacturerID);
		if(slot->isHW) {
			PR_fprintf(PR_STDOUT, PAD"Type: Hardware\n");
		} else {
			PR_fprintf(PR_STDOUT, PAD"Type: Software\n");
		}
		PR_fprintf(PR_STDOUT, PAD"Version Number: %d.%d\n",
			slotinfo.hardwareVersion.major, slotinfo.hardwareVersion.minor);
		PR_fprintf(PR_STDOUT, PAD"Firmware Version: %d.%d\n",
			slotinfo.firmwareVersion.major, slotinfo.firmwareVersion.minor);
		if(slot->disabled) {
			reason  = PK11_GetDisabledReason(slot);
			if(reason < numDisableReasonStr) {
				PR_fprintf(PR_STDOUT, PAD"Status: DISABLED (%s)\n",
				  disableReasonStr[reason]);
			} else {
				PR_fprintf(PR_STDOUT, PAD"Status: DISABLED\n");
			}
		} else {
			PR_fprintf(PR_STDOUT, PAD"Status: Enabled\n");
		}

		/* Token Info */
		PR_fprintf(PR_STDOUT, PAD"Token Name: %.32s\n",
			tokeninfo.label);
		PR_fprintf(PR_STDOUT, PAD"Token Manufacturer: %.32s\n",
			tokeninfo.manufacturerID);
		PR_fprintf(PR_STDOUT, PAD"Token Model: %.16s\n", tokeninfo.model);
		PR_fprintf(PR_STDOUT, PAD"Token Serial Number: %.16s\n",
			tokeninfo.serialNumber);
		PR_fprintf(PR_STDOUT, PAD"Token Version: %d.%d\n",
			tokeninfo.hardwareVersion.major, tokeninfo.hardwareVersion.minor);
		PR_fprintf(PR_STDOUT, PAD"Token Firmware Version: %d.%d\n",
			tokeninfo.firmwareVersion.major, tokeninfo.firmwareVersion.minor);
		if(tokeninfo.flags & CKF_WRITE_PROTECTED) {
			PR_fprintf(PR_STDOUT, PAD"Access: Write Protected\n");
		} else {
			PR_fprintf(PR_STDOUT, PAD"Access: NOT Write Protected\n");
		}
		if(tokeninfo.flags & CKF_LOGIN_REQUIRED) {
			PR_fprintf(PR_STDOUT, PAD"Login Type: Login required\n");
		} else {
			PR_fprintf(PR_STDOUT, PAD
			  "Login Type: Public (no login required)\n");
		}
		if(tokeninfo.flags & CKF_USER_PIN_INITIALIZED) {
			PR_fprintf(PR_STDOUT, PAD"User Pin: Initialized\n");
		} else {
			PR_fprintf(PR_STDOUT, PAD"User Pin: NOT Initialized\n");
		}
	}
	PR_fprintf(PR_STDOUT, 
		"\n-----------------------------------------------------------\n");
	return SUCCESS;
}

/************************************************************************
 *
 * C h a n g e P W
 */
Error
ChangePW(char *tokenName, char *pwFile, char *newpwFile)
{
	char *oldpw=NULL, *newpw=NULL, *newpw2=NULL;
	PK11SlotInfo *slot;
	Error ret=UNSPECIFIED_ERR;
	PRBool matching;

	slot = PK11_FindSlotByName(tokenName);
	if(!slot) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_TOKEN_ERR], tokenName);
		return NO_SUCH_TOKEN_ERR;
	}

	PK11_SetPasswordFunc(SECU_GetModulePassword);

	/* Get old password */
	if(! PK11_NeedUserInit(slot)) {
		if(pwFile) {
			oldpw = SECU_FilePasswd(NULL, PR_FALSE, pwFile);
			if(PK11_CheckUserPassword(slot, oldpw) != SECSuccess) {
				PR_fprintf(PR_STDERR, errStrings[BAD_PW_ERR]);
				ret=BAD_PW_ERR;
				goto loser;
			}
		} else {
			for(matching=PR_FALSE; !matching; ) {
				oldpw = SECU_GetPasswordString(NULL, "Enter old password: ");
				if(PK11_CheckUserPassword(slot, oldpw) == SECSuccess) {
					matching = PR_TRUE;
				} else {
					PR_fprintf(PR_STDOUT, msgStrings[BAD_PW_MSG]);
				}
			}
		}
	}

	/* Get new password */
	if(newpwFile) {
		newpw = SECU_FilePasswd(NULL, PR_FALSE, newpwFile);
	} else {
		for(matching=PR_FALSE; !matching; ) {
			newpw = SECU_GetPasswordString(NULL, "Enter new password: ");
			newpw2 = SECU_GetPasswordString(NULL, "Re-enter new password: ");
			if(strcmp(newpw, newpw2)) {
				PR_fprintf(PR_STDOUT, msgStrings[PW_MATCH_MSG]);
			} else {
				matching = PR_TRUE;
			}
		}
	}

	/* Change the password */
	if(PK11_NeedUserInit(slot)) {
		if(PK11_InitPin(slot, NULL /*ssopw*/, newpw) != SECSuccess) {
			PR_fprintf(PR_STDERR, errStrings[CHANGEPW_FAILED_ERR], tokenName);
			ret = CHANGEPW_FAILED_ERR;
			goto loser;
		}
	} else {
		if(PK11_ChangePW(slot, oldpw, newpw) != SECSuccess) {
			PR_fprintf(PR_STDERR, errStrings[CHANGEPW_FAILED_ERR], tokenName);
			ret = CHANGEPW_FAILED_ERR;
			goto loser;
		}
	}

	PR_fprintf(PR_STDOUT, msgStrings[CHANGEPW_SUCCESS_MSG], tokenName);
	ret = SUCCESS;

loser:
	if(oldpw) {
		memset(oldpw, 0, strlen(oldpw));
		PORT_Free(oldpw);
	}
	if(newpw) {
		memset(newpw, 0, strlen(newpw));
		PORT_Free(newpw);
	}
	if(newpw2) {
		memset(newpw2, 0, strlen(newpw));
		PORT_Free(newpw2);
	}
	return ret;
}

/***********************************************************************
 *
 * E n a b l e M o d u l e
 *
 * If enable==PR_TRUE, enables the module or slot.
 * If enable==PR_FALSE, disables the module or slot.
 * moduleName is the name of the module.
 * slotName is the name of the slot.  It is optional.
 */
Error
EnableModule(char *moduleName, char *slotName, PRBool enable)
{
	int i;
	SECMODModule *module;
	PK11SlotInfo *slot = NULL;
	PRBool found = PR_FALSE;

	module = SECMOD_FindModule(moduleName);
	if(!module) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_MODULE_ERR], moduleName);
		return NO_SUCH_MODULE_ERR;
	}

	for(i=0; i < module->slotCount; i++) {
		slot = module->slots[i];
		if(slotName && strcmp(PK11_GetSlotName(slot), slotName)) {
				/* Not the right slot */
				continue;
		}
		if(enable) {
			if(! PK11_UserEnableSlot(slot)) {
				PR_fprintf(PR_STDERR, errStrings[ENABLE_FAILED_ERR],
					"enable", PK11_GetSlotName(slot));
				return ENABLE_FAILED_ERR;
			} else {
				found = PR_TRUE;
				PR_fprintf(PR_STDOUT, msgStrings[ENABLE_SUCCESS_MSG],
					PK11_GetSlotName(slot), "enabled");
			}
		} else {
			if(! PK11_UserDisableSlot(slot)) {
				PR_fprintf(PR_STDERR, errStrings[ENABLE_FAILED_ERR],
					"disable", PK11_GetSlotName(slot));
				return ENABLE_FAILED_ERR;
			} else {
				found = PR_TRUE;
				PR_fprintf(PR_STDOUT, msgStrings[ENABLE_SUCCESS_MSG],
					PK11_GetSlotName(slot), "disabled");
			}
		}
	}

	if(slotName && !found) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_SLOT_ERR], slotName);
		return NO_SUCH_SLOT_ERR;
	}

	/* Delete and re-add module to save changes */
	if( SECMOD_UpdateModule(module) != SECSuccess ) {
		PR_fprintf(PR_STDERR, errStrings[UPDATE_MOD_FAILED_ERR], moduleName);
		return UPDATE_MOD_FAILED_ERR;
	}

	return SUCCESS;
}

/*************************************************************************
 *
 * S e t D e f a u l t M o d u l e
 *
 */
Error
SetDefaultModule(char *moduleName, char *slotName, char *mechanisms)
{
	SECMODModule *module;
	PK11SlotInfo *slot;
	int s, i;
	unsigned long mechFlags = getFlagsFromString(mechanisms, mechanismStrings,
		numMechanismStrings);
	PRBool found = PR_FALSE;
	Error errcode = UNSPECIFIED_ERR;

	mechFlags =  SECMOD_PubMechFlagstoInternal(mechFlags);

	module = SECMOD_FindModule(moduleName);
	if(!module) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_MODULE_ERR], moduleName);
		errcode = NO_SUCH_MODULE_ERR;
		goto loser;
	}

	/* Go through each slot */
	for(s=0; s < module->slotCount; s++) {
		slot = module->slots[s];

		if ((slotName != NULL) &&
			!((strcmp(PK11_GetSlotName(slot),slotName) == 0) ||
			(strcmp(PK11_GetTokenName(slot),slotName) == 0)) ) {
		    /* we are only interested in changing the one slot */
		    continue;
		}

		found = PR_TRUE;

		/* Go through each mechanism */
		for(i=0; i < num_pk11_default_mechanisms; i++) {
			if(PK11_DefaultArray[i].flag & mechFlags) {
				/* Enable this default mechanism */
				PK11_UpdateSlotAttribute(slot, &(PK11_DefaultArray[i]),
					PR_TRUE);
			}
		}
	}
	if (slotName && !found) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_SLOT_ERR], slotName);
		errcode = NO_SUCH_SLOT_ERR;
		goto loser;
	}

	/* Delete and re-add module to save changes */
	if( SECMOD_UpdateModule(module) != SECSuccess ) {
		PR_fprintf(PR_STDERR, errStrings[DEFAULT_FAILED_ERR],
		  moduleName);
		errcode = DEFAULT_FAILED_ERR;
		goto loser;
	}

	PR_fprintf(PR_STDOUT, msgStrings[DEFAULT_SUCCESS_MSG]);

	errcode = SUCCESS;
loser:
	return errcode;
}

/************************************************************************
 *
 * U n s e t D e f a u l t M o d u l e
 */
Error
UnsetDefaultModule(char *moduleName, char *slotName, char *mechanisms)
{
	SECMODModule * module;
	PK11SlotInfo *slot;
	int s, i;
	unsigned long mechFlags = getFlagsFromString(mechanisms,
		mechanismStrings, numMechanismStrings);
	PRBool found = PR_FALSE;

	mechFlags =  SECMOD_PubMechFlagstoInternal(mechFlags);

	module = SECMOD_FindModule(moduleName);
	if(!module) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_MODULE_ERR], moduleName);
		return NO_SUCH_MODULE_ERR;
	}

	for(s=0; s < module->slotCount; s++) {
		slot = module->slots[s];
		if ((slotName != NULL) &&
			!((strcmp(PK11_GetSlotName(slot),slotName) == 0) ||
			(strcmp(PK11_GetTokenName(slot),slotName) == 0)) ) {
		    /* we are only interested in changing the one slot */
		    continue;
		}
		for(i=0; i <num_pk11_default_mechanisms; i++) {
			if(PK11_DefaultArray[i].flag & mechFlags) {
				PK11_UpdateSlotAttribute(slot, &(PK11_DefaultArray[i]),
					PR_FALSE);
			}
		}
	}
	if (slotName && !found) {
		PR_fprintf(PR_STDERR, errStrings[NO_SUCH_SLOT_ERR], slotName);
		return NO_SUCH_SLOT_ERR;
	}

	/* Delete and re-add module to save changes */
	if( SECMOD_UpdateModule(module) != SECSuccess ) {
		PR_fprintf(PR_STDERR, errStrings[UNDEFAULT_FAILED_ERR],
		  moduleName);
		return UNDEFAULT_FAILED_ERR;
	}

	PR_fprintf(PR_STDOUT, msgStrings[UNDEFAULT_SUCCESS_MSG]);
	return SUCCESS;
}
