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
#include "install.h"
#include <plstr.h>
#include "secrng.h"
#include "certdb.h" /* for CERT_DB_FILE_VERSION */
#include "nss.h"

static void install_error(char *message);
static char* PR_fgets(char *buf, int size, PRFileDesc *file);
static char *progName;


/* This enum must be kept in sync with the commandNames list */
typedef enum {
	NO_COMMAND,
	ADD_COMMAND,
	CHANGEPW_COMMAND,
	CREATE_COMMAND,
	DEFAULT_COMMAND,
	DELETE_COMMAND,
	DISABLE_COMMAND,
	ENABLE_COMMAND,
	FIPS_COMMAND,
	JAR_COMMAND,
	LIST_COMMAND,
	UNDEFAULT_COMMAND
} Command;

/* This list must be kept in sync with the Command enum */
static char *commandNames[] = {
	"(no command)",
	"-add",
	"-changepw",
	"-create",
	"-default",
	"-delete",
	"-disable",
	"-enable",
	"-fips",
	"-jar",
	"-list",
	"-undefault"
};


/* this enum must be kept in sync with the optionStrings list */
typedef enum {
	ADD_ARG=0,
	CHANGEPW_ARG,
	CIPHERS_ARG,
	CREATE_ARG,
	DBDIR_ARG,
	DBPREFIX_ARG,
	DEFAULT_ARG,
	DELETE_ARG,
	DISABLE_ARG,
	ENABLE_ARG,
	FIPS_ARG,
	FORCE_ARG,
	JAR_ARG,
	LIBFILE_ARG,
	LIST_ARG,
	MECHANISMS_ARG,
	NEWPWFILE_ARG,
	PWFILE_ARG,
	SLOT_ARG,
	UNDEFAULT_ARG,
	INSTALLDIR_ARG,
	TEMPDIR_ARG,
	NOCERTDB_ARG,

	NUM_ARGS	/* must be last */
} Arg;

/* This list must be kept in sync with the Arg enum */
static char *optionStrings[] = {
	"-add",
	"-changepw",
	"-ciphers",
	"-create",
	"-dbdir",
	"-dbprefix",
	"-default",
	"-delete",
	"-disable",
	"-enable",
	"-fips",
	"-force",
	"-jar",
	"-libfile",
	"-list",
	"-mechanisms",
	"-newpwfile",
	"-pwfile",
	"-slot",
	"-undefault",
	"-installdir",
	"-tempdir",
	"-nocertdb"
};

/* Increment i if doing so would have i still be less than j.  If you
   are able to do this, return 0.  Otherwise return 1. */
#define TRY_INC(i,j)  ( ((i+1)<j) ? (++i, 0) : 1 )

/********************************************************************
 *
 * file-wide variables obtained from the command line
 */
static Command command = NO_COMMAND;
static char* pwFile = NULL;
static char* newpwFile = NULL;
static char* moduleName = NULL;
static char* slotName = NULL;
static char* tokenName = NULL;
static char* libFile = NULL;
static char* dbdir = NULL;
static char* dbprefix = "";
static char* mechanisms = NULL;
static char* ciphers = NULL;
static char* fipsArg = NULL;
static char* jarFile = NULL;
static char* installDir = NULL;
static char* tempDir = NULL;
static short force = 0;
static PRBool nocertdb = PR_FALSE;

/*******************************************************************
 *
 * p a r s e _ a r g s
 */
static Error
parse_args(int argc, char *argv[])
{
	int i;
	char *arg;
	int optionType;

	/* Loop over all arguments */
	for(i=1; i < argc; i++) {
		arg = argv[i];

		/* Make sure this is an option and not some floating argument */
		if(arg[0] != '-') {
			PR_fprintf(PR_STDERR, errStrings[UNEXPECTED_ARG_ERR], argv[i]);
			return UNEXPECTED_ARG_ERR;
		}

		/* Find which option this is */
		for(optionType=0; optionType < NUM_ARGS; optionType++) {
			if(! strcmp(arg, optionStrings[optionType])) {
				break;
			}
		}
		
		/* Deal with this specific option */
		switch(optionType) {
		case NUM_ARGS:
		default:
			PR_fprintf(PR_STDERR, errStrings[UNKNOWN_OPTION_ERR], arg);
			return UNKNOWN_OPTION_ERR;
			break;
		case ADD_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = ADD_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			moduleName = argv[i];
			break;
		case CHANGEPW_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = CHANGEPW_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			tokenName = argv[i];
			break;
		case CIPHERS_ARG:
			if(ciphers != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			ciphers = argv[i];
			break;
		case CREATE_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = CREATE_COMMAND;
			break;
		case DBDIR_ARG:
			if(dbdir != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			dbdir = argv[i];
			break;
		case DBPREFIX_ARG:
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			dbprefix = argv[i];
			break;
		case UNDEFAULT_ARG:
		case DEFAULT_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			if(optionType == DEFAULT_ARG) {
				command = DEFAULT_COMMAND;
			} else {
				command = UNDEFAULT_COMMAND;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			moduleName = argv[i];
			break;
		case DELETE_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = DELETE_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			moduleName = argv[i];
			break;
		case DISABLE_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = DISABLE_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			moduleName = argv[i];
			break;
		case ENABLE_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = ENABLE_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			moduleName = argv[i];
			break;
		case FIPS_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = FIPS_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			fipsArg = argv[i];
			break;
		case FORCE_ARG:
			force = 1;
			break;
		case NOCERTDB_ARG:
			nocertdb = PR_TRUE;
			break;
		case INSTALLDIR_ARG:
			if(installDir != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			installDir = argv[i];
			break;
		case TEMPDIR_ARG:
			if(tempDir != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			tempDir = argv[i];
			break;
		case JAR_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = JAR_COMMAND;
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			jarFile = argv[i];
			break;
		case LIBFILE_ARG:
			if(libFile != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			libFile = argv[i];
			break;
		case LIST_ARG:
			if(command != NO_COMMAND) {
				PR_fprintf(PR_STDERR, errStrings[MULTIPLE_COMMAND_ERR], arg);
				return MULTIPLE_COMMAND_ERR;
			}
			command = LIST_COMMAND;
			/* This option may or may not have an argument */
			if( (i+1 < argc) && (argv[i+1][0] != '-') ) {
					moduleName = argv[++i];
			}
			break;
		case MECHANISMS_ARG:
			if(mechanisms != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			mechanisms = argv[i];
			break;
		case NEWPWFILE_ARG:
			if(newpwFile != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			newpwFile = argv[i];
			break;
		case PWFILE_ARG:
			if(pwFile != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			pwFile = argv[i];
			break;
		case SLOT_ARG:
			if(slotName != NULL) {
				PR_fprintf(PR_STDERR, errStrings[DUPLICATE_OPTION_ERR], arg);
				return DUPLICATE_OPTION_ERR;
			}
			if(TRY_INC(i, argc)) {
				PR_fprintf(PR_STDERR, errStrings[OPTION_NEEDS_ARG_ERR], arg);
				return OPTION_NEEDS_ARG_ERR;
			}
			slotName = argv[i];
			break;
		}
	}
	return SUCCESS;
}

/************************************************************************
 *
 * v e r i f y _ p a r a m s
 */
static Error
verify_params()
{
	switch(command) {
	case ADD_COMMAND:
		if(libFile == NULL) {
			PR_fprintf(PR_STDERR, errStrings[MISSING_PARAM_ERR],
				commandNames[ADD_COMMAND], optionStrings[LIBFILE_ARG]);
			return MISSING_PARAM_ERR;
		}
		break;
	case CHANGEPW_COMMAND:
		break;
	case CREATE_COMMAND:
		break;
	case DELETE_COMMAND:
		break;
	case DISABLE_COMMAND:
		break;
	case ENABLE_COMMAND:
		break;
	case FIPS_COMMAND:
		if(PL_strcasecmp(fipsArg, "true") &&
			PL_strcasecmp(fipsArg, "false")) {
			PR_fprintf(PR_STDERR, errStrings[INVALID_FIPS_ARG]);
			return INVALID_FIPS_ARG;
		}
		break;
	case JAR_COMMAND:
		if(installDir == NULL) {
			PR_fprintf(PR_STDERR, errStrings[MISSING_PARAM_ERR],
				commandNames[JAR_COMMAND], optionStrings[INSTALLDIR_ARG]);
			return MISSING_PARAM_ERR;
		}
		break;
	case LIST_COMMAND:
		break;
	case UNDEFAULT_COMMAND:
	case DEFAULT_COMMAND:
		if(mechanisms == NULL) {
			PR_fprintf(PR_STDERR, errStrings[MISSING_PARAM_ERR],
				commandNames[command], optionStrings[MECHANISMS_ARG]);
			return MISSING_PARAM_ERR;
		}
		break;
	default:
		/* Ignore this here */
		break;
	}

	return SUCCESS;
}

/********************************************************************
 *
 * i n i t _ c r y p t o
 *
 * Does crypto initialization that all commands will require.
 * If -nocertdb option is specified, don't open key or cert db (we don't
 * need them if we aren't going to be verifying signatures).  This is
 * because serverland doesn't always have cert and key database files
 * available.
 */
static Error
init_crypto(PRBool create, PRBool readOnly)
{
	char *dir;
#ifdef notdef
	char *moddbname=NULL, *keydbname, *certdbname;
	PRBool free_moddbname = PR_FALSE;
#endif
	Error retval;
	SECStatus rv;
	PRUint32 flags = 0;


	if(SECU_ConfigDirectory(dbdir)[0] == '\0') {
		PR_fprintf(PR_STDERR, errStrings[NO_DBDIR_ERR]);
		retval=NO_DBDIR_ERR;
		goto loser;
	}
	dir = SECU_ConfigDirectory(NULL);

	/* Make sure db directory exists and is readable */
	if(PR_Access(dir, PR_ACCESS_EXISTS) != PR_SUCCESS) {
		PR_fprintf(PR_STDERR, errStrings[DIR_DOESNT_EXIST_ERR], dir);
		retval = DIR_DOESNT_EXIST_ERR;
		goto loser;
	} else if(PR_Access(dir, PR_ACCESS_READ_OK) != PR_SUCCESS) {
		PR_fprintf(PR_STDERR, errStrings[DIR_NOT_READABLE_ERR], dir);
		retval = DIR_NOT_READABLE_ERR;
		goto loser;
	}

	/* Check for the proper permissions on databases */
	if(create) {
		/* Make sure dbs don't already exist, and the directory is
			writeable */
#ifdef notdef
		if(PR_Access(moddbname, PR_ACCESS_EXISTS)==PR_SUCCESS) {
			PR_fprintf(PR_STDERR, errStrings[FILE_ALREADY_EXISTS_ERR],
			  moddbname);
			retval=FILE_ALREADY_EXISTS_ERR;
			goto loser;
		} else if(PR_Access(keydbname, PR_ACCESS_EXISTS)==PR_SUCCESS) {
			PR_fprintf(PR_STDERR, errStrings[FILE_ALREADY_EXISTS_ERR], keydbname);
			retval=FILE_ALREADY_EXISTS_ERR;
			goto loser;
		} else if(PR_Access(certdbname, PR_ACCESS_EXISTS)==PR_SUCCESS) {
			PR_fprintf(PR_STDERR, errStrings[FILE_ALREADY_EXISTS_ERR],certdbname);
			retval=FILE_ALREADY_EXISTS_ERR;
			goto loser;
		} else 
#endif
		if(PR_Access(dir, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
			PR_fprintf(PR_STDERR, errStrings[DIR_NOT_WRITEABLE_ERR], dir);
			retval=DIR_NOT_WRITEABLE_ERR;
			goto loser;
		}
	} else {
#ifdef notdef
		/* Make sure dbs are readable and writeable */
		if(PR_Access(moddbname, PR_ACCESS_READ_OK) != PR_SUCCESS) {
#ifndef XP_PC
			/* in serverland, they always use secmod.db, even on UNIX. Try
			   this */
			moddbname = PR_smprintf("%s/secmod.db", dir);
			free_moddbname = PR_TRUE;
			if(PR_Access(moddbname, PR_ACCESS_READ_OK) != PR_SUCCESS) {
#endif
			PR_fprintf(PR_STDERR, errStrings[FILE_NOT_READABLE_ERR], moddbname);
			retval=FILE_NOT_READABLE_ERR;
			goto loser;
#ifndef XP_PC
			}
#endif
		}
		if(!nocertdb) { /* don't open cert and key db if -nocertdb */
			if(PR_Access(keydbname, PR_ACCESS_READ_OK) != PR_SUCCESS) {
				PR_fprintf(PR_STDERR, errStrings[FILE_NOT_READABLE_ERR],
				  keydbname);
				retval=FILE_NOT_READABLE_ERR;
				goto loser;
			}
			if(PR_Access(certdbname, PR_ACCESS_READ_OK) != PR_SUCCESS) {
				PR_fprintf(PR_STDERR, errStrings[FILE_NOT_READABLE_ERR],
				  certdbname);
				retval=FILE_NOT_READABLE_ERR;
				goto loser;
			}
		}
#endif

		/* Check for write access if we'll be making changes */
		if( !readOnly ) {
#ifdef notdef
			if(PR_Access(moddbname, PR_ACCESS_WRITE_OK) != PR_SUCCESS) {
				PR_fprintf(PR_STDERR, errStrings[FILE_NOT_WRITEABLE_ERR],
				  moddbname);
				retval=FILE_NOT_WRITEABLE_ERR;
				goto loser;
			}
			if(!nocertdb) { /* don't open key and cert db if -nocertdb */
				if(PR_Access(keydbname, PR_ACCESS_WRITE_OK)
															!= PR_SUCCESS) {
					PR_fprintf(PR_STDERR, errStrings[FILE_NOT_WRITEABLE_ERR],
					  keydbname);
					retval=FILE_NOT_WRITEABLE_ERR;
					goto loser;
				}
				if(PR_Access(certdbname, PR_ACCESS_WRITE_OK)
															!= PR_SUCCESS) {
					PR_fprintf(PR_STDERR, errStrings[FILE_NOT_WRITEABLE_ERR],
					  certdbname);
					retval=FILE_NOT_WRITEABLE_ERR;
					goto loser;
				}
			}
#endif
		}
		PR_fprintf(PR_STDOUT, msgStrings[USING_DBDIR_MSG],
		  SECU_ConfigDirectory(NULL));
	}

	/* Open/create key database */
	flags = 0;
	if (readOnly) flags |= NSS_INIT_READONLY;
	if (nocertdb) flags |= NSS_INIT_NOCERTDB;
	rv = NSS_Initialize(SECU_ConfigDirectory(NULL), dbprefix, dbprefix,
	               "secmod.db", flags);
	if (rv != SECSuccess) {
	    SECU_PrintPRandOSError(progName);
	    retval=NSS_INITIALIZE_FAILED_ERR;
	    goto loser;
	}

	retval=SUCCESS;
loser:
#ifdef notdef
	if(free_moddbname) {
		PR_Free(moddbname);
	}
#endif
	return retval;
}

/*************************************************************************
 *
 * u s a g e
 */
static void
usage()
{
	PR_fprintf(PR_STDOUT,
"\nNetscape Cryptographic Module Utility\n"
"Usage: modutil [command] [options]\n\n"
"                            COMMANDS\n"
"---------------------------------------------------------------------------\n"
"-add MODULE_NAME                 Add the named module to the module database\n"
"   -libfile LIBRARY_FILE         The name of the file (.so or .dll)\n"
"                                 containing the implementation of PKCS #11\n"
"   [-ciphers CIPHER_LIST]        Enable the given ciphers on this module\n"
"   [-mechanisms MECHANISM_LIST]  Make the module a default provider of the\n"
"                                 given mechanisms\n"
"-changepw TOKEN                  Change the password on the named token\n"
"   [-pwfile FILE]                The old password is in this file\n"
"   [-newpwfile FILE]             The new password is in this file\n"
"-create                          Create a new set of security databases\n"
"-default MODULE                  Make the given module a default provider\n"
"   -mechanisms MECHANISM_LIST    of the given mechanisms\n"
"   [-slot SLOT]                  limit change to only the given slot\n"
"-delete MODULE                   Remove the named module from the module\n"
"                                 database\n"
"-disable MODULE                  Disable the named module\n"
"   [-slot SLOT]                  Disable only the named slot on the module\n"
"-enable MODULE                   Enable the named module\n"
"   [-slot SLOT]                  Enable only the named slot on the module\n"
"-fips [ true | false ]           If true, enable FIPS mode.  If false,\n"
"                                 disable FIPS mode\n"
"-force                           Do not run interactively\n"
"-jar JARFILE                     Install a PKCS #11 module from the given\n"
"                                 JAR file in the PKCS #11 JAR format\n"
"   -installdir DIR               Use DIR as the root directory of the\n"
"                                 installation\n"
"   [-tempdir DIR]                Use DIR as the temporary installation\n"
"                                 directory. If not specified, the current\n"
"                                 directory is used\n"
"-list [MODULE]                   Lists information about the specified module\n"
"                                 or about all modules if none is specified\n"
"-undefault MODULE                The given module is NOT a default provider\n"
"   -mechanisms MECHANISM_LIST    of the listed mechanisms\n"
"   [-slot SLOT]                  limit change to only the given slot\n"
"---------------------------------------------------------------------------\n"
"\n"
"                             OPTIONS\n"
"---------------------------------------------------------------------------\n"
"-dbdir DIR                       Directory DIR contains the security databases\n"
"-dbprefix prefix                 Prefix for the security databases\n"
"-nocertdb                        Do not load certificate or key databases. No\n"
"                                 verification will be performed on JAR files.\n"
"---------------------------------------------------------------------------\n"
"\n"
"Mechanism lists are colon-separated.  The following mechanisms are recognized:\n"
"RSA, DSA, RC2, RC4, RC5, DES, DH, FORTEZZA, SHA1, MD5, MD2, SSL, TLS, RANDOM,\n"
" FRIENDLY\n"
"\n"
"Cipher lists are colon-separated.  The following ciphers are recognized:\n"
"FORTEZZA\n"
"\nQuestions or bug reports should be sent to modutil-support@netscape.com.\n"
);

}

/*************************************************************************
 *
 * m a i n
 */
int
main(int argc, char *argv[])
{
	int errcode = SUCCESS;
	PRBool createdb, readOnly;
#define STDINBUF_SIZE 80
	char stdinbuf[STDINBUF_SIZE];

    progName = strrchr(argv[0], '/');
    progName = progName ? progName+1 : argv[0];


	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

	if(parse_args(argc, argv) != SUCCESS) {
		usage();
		errcode = INVALID_USAGE_ERR;
		goto loser;
	}

	if(verify_params() != SUCCESS) {
		usage();
		errcode = INVALID_USAGE_ERR;
		goto loser;
	}

	if(command==NO_COMMAND) {
		PR_fprintf(PR_STDERR, errStrings[NO_COMMAND_ERR]);
		usage();
		errcode = INVALID_USAGE_ERR;
		goto loser;
	}

	/* Set up crypto stuff */
	createdb = command==CREATE_COMMAND;
	readOnly = command==LIST_COMMAND;

	/* Make sure browser is not running if we're writing to a database */
	/* Do this before initializing crypto */
	if(!readOnly && !force) {
		char *response;

		PR_fprintf(PR_STDOUT, msgStrings[BROWSER_RUNNING_MSG]);
		if( ! PR_fgets(stdinbuf, STDINBUF_SIZE, PR_STDIN)) {
			PR_fprintf(PR_STDERR, errStrings[STDIN_READ_ERR]);
			errcode = STDIN_READ_ERR;
			goto loser;
		}
		if( (response=strtok(stdinbuf, " \r\n\t")) ) {
			if(!PL_strcasecmp(response, "q")) {
				PR_fprintf(PR_STDOUT, msgStrings[ABORTING_MSG]);
				errcode = SUCCESS;
				goto loser;
			}
		}
		PR_fprintf(PR_STDOUT, "\n");
	}

	errcode = init_crypto(createdb, readOnly);
	if( errcode != SUCCESS) {
		goto loser;
	}


	/* Execute the command */
	switch(command) {
	case ADD_COMMAND:
		errcode = AddModule(moduleName, libFile, ciphers, mechanisms);
		break;
	case CHANGEPW_COMMAND:
		errcode = ChangePW(tokenName, pwFile, newpwFile);
		break;
	case CREATE_COMMAND:
		/* The work was already done in init_crypto() */
		break;
	case DEFAULT_COMMAND:
		errcode = SetDefaultModule(moduleName, slotName, mechanisms);
		break;
	case DELETE_COMMAND:
		errcode = DeleteModule(moduleName);
		break;
	case DISABLE_COMMAND:
		errcode = EnableModule(moduleName, slotName, PR_FALSE);
		break;
	case ENABLE_COMMAND:
		errcode = EnableModule(moduleName, slotName, PR_TRUE);
		break;
	case FIPS_COMMAND:
		errcode = FipsMode(fipsArg);
		break;
	case JAR_COMMAND:
		Pk11Install_SetErrorHandler(install_error);
		errcode = Pk11Install_DoInstall(jarFile, installDir, tempDir,
						PR_STDOUT, force, nocertdb);
		break;
	case LIST_COMMAND:
		if(moduleName) {
			errcode = ListModule(moduleName);
		} else {
			errcode = ListModules();
		}
		break;
	case UNDEFAULT_COMMAND:
		errcode = UnsetDefaultModule(moduleName, slotName, mechanisms);
		break;
	default:
		PR_fprintf(PR_STDERR, "This command is not supported yet.\n");
		errcode = INVALID_USAGE_ERR;
		break;
	}

loser:
	PR_Cleanup();
	return errcode;
}

/************************************************************************
 *
 * i n s t a l l _ e r r o r
 *
 * Callback function to handle errors in PK11 JAR file installation.
 */
static void
install_error(char *message)
{
	PR_fprintf(PR_STDERR, "Install error: %s\n", message);
}

/*************************************************************************
 *
 * o u t _ o f _ m e m o r y
 */
void
out_of_memory(void)
{
	PR_fprintf(PR_STDERR, errStrings[OUT_OF_MEM_ERR]);
	exit(OUT_OF_MEM_ERR);
}


/**************************************************************************
 *
 * P R _ f g e t s
 *
 * fgets implemented with NSPR.
 */
static char*
PR_fgets(char *buf, int size, PRFileDesc *file)
{
	int i;
	int status;
	char c;

	i=0;
	while(i < size-1) {
		status = PR_Read(file, (void*) &c, 1);
		if(status==-1) {
			return NULL;
		} else if(status==0) {
			break;
		}
		buf[i++] = c;
		if(c=='\n') {
			break;
		}
	}
	buf[i]='\0';

	return buf;
}
