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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 *  SIGNTOOL
 *
 *  A command line tool to create manifest files
 *  from a directory hierarchy. It is assumed that
 *  the tree will be equivalent to what resides
 *  or will reside in an archive. 
 *
 * 
 */

#include "nss.h"
#include "signtool.h"
#include "prmem.h"
#include "prio.h"

/***********************************************************************
 * Global Variable Definitions
 */
char	*progName; /* argv[0] */

/* password on command line. Use for build testing only */
char	*password = NULL;

/* directories or files to exclude in descent */
PLHashTable *excludeDirs = NULL;
static PRBool exclusionsGiven = PR_FALSE;

/* zatharus is the man who knows no time, dies tragic death */
int	no_time = 0;

/* -b basename of .rsa, .sf files */
char	*base = DEFAULT_BASE_NAME;

/* Only sign files with this extension */
PLHashTable *extensions = NULL;
PRBool extensionsGiven = PR_FALSE;

char	*scriptdir = NULL;

int	verbosity = 0;

PRFileDesc *outputFD = NULL, *errorFD = NULL;

int	errorCount = 0, warningCount = 0;

int	compression_level = DEFAULT_COMPRESSION_LEVEL;
PRBool compression_level_specified = PR_FALSE;

int	xpi_arc = 0;

/* Command-line arguments */
static char	*genkey = NULL;
static char	*verify = NULL;
static char	*zipfile = NULL;
static char	*cert_dir = NULL;
static int	javascript = 0;
static char	*jartree = NULL;
static char	*keyName = NULL;
static char	*metafile = NULL;
static char	*install_script = NULL;
static int	list_certs = 0;
static int	list_modules = 0;
static int	optimize = 0;
static int	enableOCSP = 0;
static char	*tell_who = NULL;
static char	*outfile = NULL;
static char	*cmdFile = NULL;
static PRBool noRecurse = PR_FALSE;
static PRBool leaveArc = PR_FALSE;
static int	keySize = -1;
static char	*token = NULL;

typedef enum {
    UNKNOWN_OPT,
    QUESTION_OPT,
    BASE_OPT,
    COMPRESSION_OPT,
    CERT_DIR_OPT,
    EXTENSION_OPT,
    INSTALL_SCRIPT_OPT,
    SCRIPTDIR_OPT,
    CERTNAME_OPT,
    LIST_OBJSIGN_CERTS_OPT,
    LIST_ALL_CERTS_OPT,
    METAFILE_OPT,
    OPTIMIZE_OPT,
    ENABLE_OCSP_OPT,
    PASSWORD_OPT,
    VERIFY_OPT,
    WHO_OPT,
    EXCLUDE_OPT,
    NO_TIME_OPT,
    JAVASCRIPT_OPT,
    ZIPFILE_OPT,
    GENKEY_OPT,
    MODULES_OPT,
    NORECURSE_OPT,
    SIGNDIR_OPT,
    OUTFILE_OPT,
    COMMAND_FILE_OPT,
    LEAVE_ARC_OPT,
    VERBOSITY_OPT,
    KEYSIZE_OPT,
    TOKEN_OPT,
    XPI_ARC_OPT
} 


OPT_TYPE;

typedef enum {
    DUPLICATE_OPTION_ERR = 0,
    OPTION_NEEDS_ARG_ERR
} 


Error;

static char	*errStrings[] = {
    "warning: %s option specified more than once.\n"
    "Only last specification will be used.\n",
    "ERROR: option \"%s\" requires an argument.\n"
};


static int	ProcessOneOpt(OPT_TYPE type, char *arg);

/*********************************************************************
 *
 * P r o c e s s C o m m a n d F i l e
 */
int
ProcessCommandFile()
{
    PRFileDesc * fd;
#define CMD_FILE_BUFSIZE 1024
    char	buf[CMD_FILE_BUFSIZE];
    char	*equals;
    int	linenum = 0;
    int	retval = -1;
    OPT_TYPE type;

    fd = PR_Open(cmdFile, PR_RDONLY, 0777);
    if (!fd) {
	PR_fprintf(errorFD, "ERROR: Unable to open command file %s.\n");
	errorCount++;
	return - 1;
    }

    while (pr_fgets(buf, CMD_FILE_BUFSIZE, fd), buf && *buf != '\0') {
	char	*eol;
	linenum++;

	/* Chop off final newline */
	eol = PL_strchr(buf, '\r');
	if (!eol) {
	    eol = PL_strchr(buf, '\n');
	}
	if (eol) 
	    *eol = '\0';

	equals = PL_strchr(buf, '=');
	if (!equals) {
	    continue;
	}

	*equals = '\0';
	equals++;

	/* Now buf points to the attribute, and equals points to the value. */

	/* This is pretty straightforward, just deal with whatever attribute
		 * this is */
	if (!PL_strcasecmp(buf, "basename")) {
	    type = BASE_OPT;
	} else if (!PL_strcasecmp(buf, "compression")) {
	    type = COMPRESSION_OPT;
	} else if (!PL_strcasecmp(buf, "certdir")) {
	    type = CERT_DIR_OPT;
	} else if (!PL_strcasecmp(buf, "extension")) {
	    type = EXTENSION_OPT;
	} else if (!PL_strcasecmp(buf, "generate")) {
	    type = GENKEY_OPT;
	} else if (!PL_strcasecmp(buf, "installScript")) {
	    type = INSTALL_SCRIPT_OPT;
	} else if (!PL_strcasecmp(buf, "javascriptdir")) {
	    type = SCRIPTDIR_OPT;
	} else if (!PL_strcasecmp(buf, "htmldir")) {
	    type = JAVASCRIPT_OPT;
	    if (jartree) {
		PR_fprintf(errorFD,
		    "warning: directory to be signed specified more than once."
		    " Only last specification will be used.\n");
		warningCount++;
		PR_Free(jartree); 
		jartree = NULL;
	    }
	    jartree = PL_strdup(equals);
	} else if (!PL_strcasecmp(buf, "certname")) {
	    type = CERTNAME_OPT;
	} else if (!PL_strcasecmp(buf, "signdir")) {
	    type = SIGNDIR_OPT;
	} else if (!PL_strcasecmp(buf, "list")) {
	    type = LIST_OBJSIGN_CERTS_OPT;
	} else if (!PL_strcasecmp(buf, "listall")) {
	    type = LIST_ALL_CERTS_OPT;
	} else if (!PL_strcasecmp(buf, "metafile")) {
	    type = METAFILE_OPT;
	} else if (!PL_strcasecmp(buf, "modules")) {
	    type = MODULES_OPT;
	} else if (!PL_strcasecmp(buf, "optimize")) {
	    type = OPTIMIZE_OPT;
	} else if (!PL_strcasecmp(buf, "ocsp")) {
	    type = ENABLE_OCSP_OPT;
	} else if (!PL_strcasecmp(buf, "password")) {
	    type = PASSWORD_OPT;
	} else if (!PL_strcasecmp(buf, "verify")) {
	    type = VERIFY_OPT;
	} else if (!PL_strcasecmp(buf, "who")) {
	    type = WHO_OPT;
	} else if (!PL_strcasecmp(buf, "exclude")) {
	    type = EXCLUDE_OPT;
	} else if (!PL_strcasecmp(buf, "notime")) {
	    type = NO_TIME_OPT;
	} else if (!PL_strcasecmp(buf, "jarfile")) {
	    type = ZIPFILE_OPT;
	} else if (!PL_strcasecmp(buf, "outfile")) {
	    type = OUTFILE_OPT;
	} else if (!PL_strcasecmp(buf, "leavearc")) {
	    type = LEAVE_ARC_OPT;
	} else if (!PL_strcasecmp(buf, "verbosity")) {
	    type = VERBOSITY_OPT;
	} else if (!PL_strcasecmp(buf, "keysize")) {
	    type = KEYSIZE_OPT;
	} else if (!PL_strcasecmp(buf, "token")) {
	    type = TOKEN_OPT;
	} else if (!PL_strcasecmp(buf, "xpi")) {
	    type = XPI_ARC_OPT;
	} else {
	    PR_fprintf(errorFD,
	        "warning: unknown attribute \"%s\" in command file, line %d.\n",
	         					buf, linenum);
	    warningCount++;
	    type = UNKNOWN_OPT;
	}

	/* Process the option, whatever it is */
	if (type != UNKNOWN_OPT) {
	    if (ProcessOneOpt(type, equals) == -1) {
		goto finish;
	    }
	}
    }

    retval = 0;

finish:
    PR_Close(fd);
    return retval;
}


/*********************************************************************
 *
 * p a r s e _ a r g s
 */
static int	
parse_args(int argc, char *argv[])
{
    char	*opt;
    char	*arg;
    int	needsInc = 0;
    int	i;
    OPT_TYPE type;

    /* Loop over all arguments */
    for (i = 1; i < argc; i++) {
	opt = argv[i];
	arg = NULL;

	if (opt[0] == '-') {
	    if (opt[1] == '-') {
		/* word option */
		if (i < argc - 1) {
		    needsInc = 1;
		    arg = argv[i+1];
		} else {
		    arg = NULL;
		}

		if ( !PL_strcasecmp(opt + 2, "norecurse")) {
		    type = NORECURSE_OPT;
		} else if ( !PL_strcasecmp(opt + 2, "leavearc")) {
		    type = LEAVE_ARC_OPT;
		} else if ( !PL_strcasecmp(opt + 2, "verbosity")) {
		    type = VERBOSITY_OPT;
		} else if ( !PL_strcasecmp(opt + 2, "outfile")) {
		    type = OUTFILE_OPT;
		} else if ( !PL_strcasecmp(opt + 2, "keysize")) {
		    type = KEYSIZE_OPT;
		} else if ( !PL_strcasecmp(opt + 2, "token")) {
		    type = TOKEN_OPT;
		} else {
		    PR_fprintf(errorFD, "warning: unknown option: %s\n",
		         opt);
		    warningCount++;
		    type = UNKNOWN_OPT;
		}
	    } else {
		/* char option */
		if (opt[2] != '\0') {
		    arg = opt + 2;
		} else if (i < argc - 1) {
		    needsInc = 1;
		    arg = argv[i+1];
		} else {
		    arg = NULL;
		}

		switch (opt[1]) {
		case '?':
		    type = QUESTION_OPT;
		    break;
		case 'b':
		    type = BASE_OPT;
		    break;
		case 'c':
		    type = COMPRESSION_OPT;
		    break;
		case 'd':
		    type = CERT_DIR_OPT;
		    break;
		case 'e':
		    type = EXTENSION_OPT;
		    break;
		case 'f':
		    type = COMMAND_FILE_OPT;
		    break;
		case 'i':
		    type = INSTALL_SCRIPT_OPT;
		    break;
		case 'j':
		    type = SCRIPTDIR_OPT;
		    break;
		case 'k':
		    type = CERTNAME_OPT;
		    break;
		case 'l':
		    type = LIST_OBJSIGN_CERTS_OPT;
		    break;
		case 'L':
		    type = LIST_ALL_CERTS_OPT;
		    break;
		case 'm':
		    type = METAFILE_OPT;
		    break;
		case 'o':
		    type = OPTIMIZE_OPT;
		    break;
		case 'O':
		    type = ENABLE_OCSP_OPT;
		    break;
		case 'p':
		    type = PASSWORD_OPT;
		    break;
		case 'v':
		    type = VERIFY_OPT;
		    break;
		case 'w':
		    type = WHO_OPT;
		    break;
		case 'x':
		    type = EXCLUDE_OPT;
		    break;
		case 'X':
		    type = XPI_ARC_OPT;
		    break;
		case 'z':
		    type = NO_TIME_OPT;
		    break;
		case 'J':
		    type = JAVASCRIPT_OPT;
		    break;
		case 'Z':
		    type = ZIPFILE_OPT;
		    break;
		case 'G':
		    type = GENKEY_OPT;
		    break;
		case 'M':
		    type = MODULES_OPT;
		    break;
		case 's':
		    type = KEYSIZE_OPT;
		    break;
		case 't':
		    type = TOKEN_OPT;
		    break;
		default:
		    type = UNKNOWN_OPT;
		    PR_fprintf(errorFD, "warning: unrecognized option: -%c.\n",
		         
		        opt[1]);
		    warningCount++;
		    break;
		}
	    }
	} else {
	    type = UNKNOWN_OPT;
	    if (i == argc - 1) {
		if (jartree) {
		    PR_fprintf(errorFD,
		        "warning: directory to be signed specified more than once.\n"
		        " Only last specification will be used.\n");
		    warningCount++;
		    PR_Free(jartree); 
		    jartree = NULL;
		}
		jartree = PL_strdup(opt);
	    } else {
		PR_fprintf(errorFD, "warning: unrecognized option: %s\n", opt);
		warningCount++;
	    }
	}

	if (type != UNKNOWN_OPT) {
	    short	ateArg = ProcessOneOpt(type, arg);
	    if (ateArg == -1) {
		/* error */
		return - 1;
	    } 
	    if (ateArg && needsInc) {
		i++;
	    }
	}
    }

    return 0;
}


/*********************************************************************
 *
 * P r o c e s s O n e O p t
 *
 * Since options can come from different places (command file, word options,
 * char options), this is a central function that is called to deal with
 * them no matter where they come from.
 *
 * type is the type of option.
 * arg is the argument to the option, possibly NULL.
 * Returns 1 if the argument was eaten, 0 if it wasn't, and -1 for error.
 */
static int	
ProcessOneOpt(OPT_TYPE type, char *arg)
{
    int	ate = 0;

    switch (type) {
    case QUESTION_OPT:
	usage();
	break;
    case BASE_OPT:
	if (base) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR], "-b");
	    warningCount++;
	    PR_Free(base); 
	    base = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR], "-b");
	    errorCount++;
	    goto loser;
	}
	base = PL_strdup(arg);
	ate = 1;
	break;
    case COMPRESSION_OPT:
	if (compression_level_specified) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR], "-c");
	    warningCount++;
	}
	if ( !arg ) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR], "-c");
	    errorCount++;
	    goto loser;
	}
	compression_level = atoi(arg);
	compression_level_specified = PR_TRUE;
	ate = 1;
	break;
    case CERT_DIR_OPT:
	if (cert_dir) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR], "-d");
	    warningCount++;
	    PR_Free(cert_dir); 
	    cert_dir = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR], "-d");
	    errorCount++;
	    goto loser;
	}
	cert_dir = PL_strdup(arg);
	ate = 1;
	break;
    case EXTENSION_OPT:
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"extension (-e)");
	    errorCount++;
	    goto loser;
	}
	PL_HashTableAdd(extensions, arg, arg);
	extensionsGiven = PR_TRUE;
	ate = 1;
	break;
    case INSTALL_SCRIPT_OPT:
	if (install_script) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"installScript (-i)");
	    warningCount++;
	    PR_Free(install_script); 
	    install_script = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"installScript (-i)");
	    errorCount++;
	    goto loser;
	}
	install_script = PL_strdup(arg);
	ate = 1;
	break;
    case SCRIPTDIR_OPT:
	if (scriptdir) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"javascriptdir (-j)");
	    warningCount++;
	    PR_Free(scriptdir); 
	    scriptdir = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"javascriptdir (-j)");
	    errorCount++;
	    goto loser;
	}
	scriptdir = PL_strdup(arg);
	ate = 1;
	break;
    case CERTNAME_OPT:
	if (keyName) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"keyName (-k)");
	    warningCount++;
	    PR_Free(keyName); 
	    keyName = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"keyName (-k)");
	    errorCount++;
	    goto loser;
	}
	keyName = PL_strdup(arg);
	ate = 1;
	break;
    case LIST_OBJSIGN_CERTS_OPT:
    case LIST_ALL_CERTS_OPT:
	if (list_certs != 0) {
	    PR_fprintf(errorFD,
	        "warning: only one of -l and -L may be specified.\n");
	    warningCount++;
	}
	list_certs = (type == LIST_OBJSIGN_CERTS_OPT ? 1 : 2);
	break;
    case METAFILE_OPT:
	if (metafile) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"metafile (-m)");
	    warningCount++;
	    PR_Free(metafile); 
	    metafile = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"metafile (-m)");
	    errorCount++;
	    goto loser;
	}
	metafile = PL_strdup(arg);
	ate = 1;
	break;
    case OPTIMIZE_OPT:
	optimize = 1;
	break;
    case ENABLE_OCSP_OPT:
	enableOCSP = 1;
	break;
    case PASSWORD_OPT:
	if (password) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"password (-p)");
	    warningCount++;
	    PR_Free(password); 
	    password = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"password (-p)");
	    errorCount++;
	    goto loser;
	}
	password = PL_strdup(arg);
	ate = 1;
	break;
    case VERIFY_OPT:
	if (verify) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"verify (-v)");
	    warningCount++;
	    PR_Free(verify); 
	    verify = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"verify (-v)");
	    errorCount++;
	    goto loser;
	}
	verify = PL_strdup(arg);
	ate = 1;
	break;
    case WHO_OPT:
	if (tell_who) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"who (-v)");
	    warningCount++;
	    PR_Free(tell_who); 
	    tell_who = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"who (-w)");
	    errorCount++;
	    goto loser;
	}
	tell_who = PL_strdup(arg);
	ate = 1;
	break;
    case EXCLUDE_OPT:
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"exclude (-x)");
	    errorCount++;
	    goto loser;
	}
	PL_HashTableAdd(excludeDirs, arg, arg);
	exclusionsGiven = PR_TRUE;
	ate = 1;
	break;
    case NO_TIME_OPT:
	no_time = 1;
	break;
    case JAVASCRIPT_OPT:
	javascript++;
	break;
    case ZIPFILE_OPT:
	if (zipfile) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"jarfile (-Z)");
	    warningCount++;
	    PR_Free(zipfile); 
	    zipfile = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"jarfile (-Z)");
	    errorCount++;
	    goto loser;
	}
	zipfile = PL_strdup(arg);
	ate = 1;
	break;
    case GENKEY_OPT:
	if (genkey) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"generate (-G)");
	    warningCount++;
	    PR_Free(zipfile); 
	    zipfile = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         				"generate (-G)");
	    errorCount++;
	    goto loser;
	}
	genkey = PL_strdup(arg);
	ate = 1;
	break;
    case MODULES_OPT:
	list_modules++;
	break;
    case SIGNDIR_OPT:
	if (jartree) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"signdir");
	    warningCount++;
	    PR_Free(jartree); 
	    jartree = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         "signdir");
	    errorCount++;
	    goto loser;
	}
	jartree = PL_strdup(arg);
	ate = 1;
	break;
    case OUTFILE_OPT:
	if (outfile) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         				"outfile");
	    warningCount++;
	    PR_Free(outfile); 
	    outfile = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         "outfile");
	    errorCount++;
	    goto loser;
	}
	outfile = PL_strdup(arg);
	ate = 1;
	break;
    case COMMAND_FILE_OPT:
	if (cmdFile) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR],
	         "-f");
	    warningCount++;
	    PR_Free(cmdFile); 
	    cmdFile = NULL;
	}
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         "-f");
	    errorCount++;
	    goto loser;
	}
	cmdFile = PL_strdup(arg);
	ate = 1;
	break;
    case NORECURSE_OPT:
	noRecurse = PR_TRUE;
	break;
    case LEAVE_ARC_OPT:
	leaveArc = PR_TRUE;
	break;
    case VERBOSITY_OPT:
	if (!arg) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR],
	         			  "--verbosity");
	    errorCount++;
	    goto loser;
	}
	verbosity = atoi(arg);
	ate = 1;
	break;
    case KEYSIZE_OPT:
	if ( keySize != -1 ) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR], "-s");
	    warningCount++;
	}
	keySize = atoi(arg);
	ate = 1;
	if ( keySize < 1 || keySize > MAX_RSA_KEY_SIZE ) {
	    PR_fprintf(errorFD, "Invalid key size: %d.\n", keySize);
	    errorCount++;
	    goto loser;
	}
	break;
    case TOKEN_OPT:
	if ( token ) {
	    PR_fprintf(errorFD, errStrings[DUPLICATE_OPTION_ERR], "-t");
	    PR_Free(token); 
	    token = NULL;
	}
	if ( !arg ) {
	    PR_fprintf(errorFD, errStrings[OPTION_NEEDS_ARG_ERR], "-t");
	    errorCount++;
	    goto loser;
	}
	token = PL_strdup(arg);
	ate = 1;
	break;
    case XPI_ARC_OPT:
	xpi_arc = 1;
	break;
    default:
	PR_fprintf(errorFD, "warning: unknown option\n");
	warningCount++;
	break;
    }

    return ate;
loser:
    return - 1;
}


/*********************************************************************
 *
 * m a i n
 */
int
main(int argc, char *argv[])
{
    PRBool readOnly;
    int	retval = 0;

    outputFD = PR_STDOUT;
    errorFD = PR_STDERR;

    progName = argv[0];

    if (argc < 2) {
	usage();
    }

    excludeDirs = PL_NewHashTable(10, PL_HashString, PL_CompareStrings,
         					PL_CompareStrings, NULL, NULL);
    extensions = PL_NewHashTable(10, PL_HashString, PL_CompareStrings,
         					PL_CompareStrings, NULL, NULL);

    if (parse_args(argc, argv)) {
	retval = -1;
	goto cleanup;
    }

    /* Parse the command file if one was given */
    if (cmdFile) {
	if (ProcessCommandFile()) {
	    retval = -1;
	    goto cleanup;
	}
    }

    /* Set up output redirection */
    if (outfile) {
	if (PR_Access(outfile, PR_ACCESS_EXISTS) == PR_SUCCESS) {
	    /* delete the file if it is already present */
	    PR_fprintf(errorFD,
	        "warning: %s already exists and will be overwritten.\n",
	         				outfile);
	    warningCount++;
	    if (PR_Delete(outfile) != PR_SUCCESS) {
		PR_fprintf(errorFD, "ERROR: unable to delete %s.\n", outfile);
		errorCount++;
		exit(ERRX);
	    }
	}
	outputFD = PR_Open(outfile,
	    PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 0777);
	if (!outputFD) {
	    PR_fprintf(errorFD, "ERROR: Unable to create %s.\n",
	         outfile);
	    errorCount++;
	    exit(ERRX);
	}
	errorFD = outputFD;
    }

    /* This seems to be a fairly common user error */

    if (verify && list_certs > 0) {
	PR_fprintf (errorFD, "%s: Can't use -l and -v at the same time\n",
	     		PROGRAM_NAME);
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    /* -J assumes -Z now */

    if (javascript && zipfile) {
	PR_fprintf (errorFD, "%s: Can't use -J and -Z at the same time\n",
	     		PROGRAM_NAME);
	PR_fprintf (errorFD, "%s: -J option will create the jar files for you\n",
	     		PROGRAM_NAME);
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    /* -X needs -Z */

    if (xpi_arc && !zipfile) {
	PR_fprintf (errorFD, "%s: option XPI (-X) requires option jarfile (-Z)\n",
	     		PROGRAM_NAME);
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    /* Less common mixing of -L with various options */

    if (list_certs > 0 && 
        (tell_who || zipfile || javascript || 
        scriptdir || extensionsGiven || exclusionsGiven || install_script)) {
	PR_fprintf(errorFD, "%s: Can't use -l or -L with that option\n",
	     			PROGRAM_NAME);
	errorCount++;
	retval = -1;
	goto cleanup;
    }


    if (!cert_dir)
	cert_dir = get_default_cert_dir();

    VerifyCertDir(cert_dir, keyName);


    if (	compression_level < MIN_COMPRESSION_LEVEL || 
        compression_level > MAX_COMPRESSION_LEVEL) {
	PR_fprintf(errorFD, "Compression level must be between %d and %d.\n",
	     		 MIN_COMPRESSION_LEVEL, MAX_COMPRESSION_LEVEL);
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    if (jartree && !keyName) {
	PR_fprintf(errorFD, "You must specify a key with which to sign.\n");
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    readOnly = (genkey == NULL); /* only key generation requires write */
    if (InitCrypto(cert_dir, readOnly)) {
	PR_fprintf(errorFD, "ERROR: Cryptographic initialization failed.\n");
	errorCount++;
	retval = -1;
	goto cleanup;
    }

    if (enableOCSP) {
	SECStatus rv = CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
	if (rv != SECSuccess) {
	    PR_fprintf(errorFD, "ERROR: Attempt to enable OCSP Checking failed.\n");
	    errorCount++;
	    retval = -1;
	}
    }

    if (verify) {
	if (VerifyJar(verify)) {
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	}
    } else if (list_certs) {
	if (ListCerts(keyName, list_certs)) {
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	}
    } else if (list_modules) {
	JarListModules();
    } else if (genkey) {
	if (GenerateCert(genkey, keySize, token)) {
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	}
    } else if (tell_who) {
	if (JarWho(tell_who)) {
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	}
    } else if (javascript && jartree) {
	/* make sure directory exists */
	PRDir * dir;
	dir = PR_OpenDir(jartree);
	if (!dir) {
	    PR_fprintf(errorFD, "ERROR: unable to open directory %s.\n",
	         jartree);
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	} else {
	    PR_CloseDir(dir);
	}

	/* undo junk from prior runs of signtool*/
	if (RemoveAllArc(jartree)) {
	    PR_fprintf(errorFD, "Error removing archive directories under %s\n",
	         jartree);
	    errorCount++;
	    retval = -1;
	    goto cleanup;
	}

	/* traverse all the htm|html files in the directory */
	if (InlineJavaScript(jartree, !noRecurse)) {
	    retval = -1;
	    goto cleanup;
	}

	/* sign any resultant .arc directories created in above step */
	if (SignAllArc(jartree, keyName, javascript, metafile, install_script,
	     		optimize, !noRecurse)) {
	    retval = -1;
	    goto cleanup;
	}

	if (!leaveArc) {
	    RemoveAllArc(jartree);
	}

	if (errorCount > 0 || warningCount > 0) {
	    PR_fprintf(outputFD, "%d error%s, %d warning%s.\n",
	         errorCount,
	        errorCount == 1 ? "" : "s", warningCount, warningCount
	        == 1 ? "" : "s");
	} else {
	    PR_fprintf(outputFD, "Directory %s signed successfully.\n",
	         jartree);
	}
    } else if (jartree) {
	SignArchive(jartree, keyName, zipfile, javascript, metafile,
	     		install_script, optimize, !noRecurse);
    } else
	usage();

cleanup:
    if (extensions) {
	PL_HashTableDestroy(extensions); 
	extensions = NULL;
    }
    if (excludeDirs) {
	PL_HashTableDestroy(excludeDirs); 
	excludeDirs = NULL;
    }
    if (outputFD != PR_STDOUT) {
	PR_Close(outputFD);
    }
    rm_dash_r(TMP_OUTPUT);
    if (retval == 0) {
	if (NSS_Shutdown() != SECSuccess) {
	    exit(1);
	}
    }
    return retval;
}


