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

#include "signtool.h"
#include "prio.h"
#include "prmem.h"
#include "nss.h"

static int is_dir (char *filename);

/***********************************************************
 * Nasty hackish function definitions
 */

long *mozilla_event_queue = 0;

#ifndef XP_WIN
char *XP_GetString (int i)
{
  return SECU_ErrorStringRaw ((int16) i);
}
#endif

void FE_SetPasswordEnabled()
{
}

void /*MWContext*/ *FE_GetInitContext (void)
{
  return 0;
}

void /*MWContext*/ *XP_FindSomeContext()
{
  /* No windows context in command tools */
  return NULL;
}

void ET_moz_CallFunction()
{
}


/*
 *  R e m o v e A l l A r c
 *
 *  Remove .arc directories that are lingering
 *  from a previous run of signtool.
 *
 */
int
RemoveAllArc(char *tree)
{
	PRDir *dir;
	PRDirEntry *entry;
	char *archive=NULL;
	int retval = 0;

	dir = PR_OpenDir (tree);
	if (!dir) return -1;

	for (entry = PR_ReadDir (dir,0); entry; entry = PR_ReadDir (dir,0)) {

		if(entry->name[0] == '.') {
			continue;
		}

		if(archive) PR_Free(archive);
		archive = PR_smprintf("%s/%s", tree, entry->name);

	    if (PL_strcaserstr (entry->name, ".arc")
			== (entry->name + strlen(entry->name) - 4) ) {

			if(verbosity >= 0) {
				PR_fprintf(outputFD, "removing: %s\n", archive);
			}

			if(rm_dash_r(archive)) {
				PR_fprintf(errorFD, "Error removing %s\n", archive);
				errorCount++;
				retval = -1;
				goto finish;
			}
		} else if(is_dir(archive)) {
			if(RemoveAllArc(archive)) {
				retval = -1;
				goto finish;
			}
		}
	}

finish:
	PR_CloseDir (dir);
	if(archive) PR_Free(archive);

	return retval;
}

/*
 *  r m _ d a s h _ r
 *
 *  Remove a file, or a directory recursively.
 *
 */
int rm_dash_r (char *path)
{
	PRDir	*dir;
	PRDirEntry *entry;
	PRFileInfo fileinfo;
	char filename[FNSIZE];

	if(PR_GetFileInfo(path, &fileinfo) != PR_SUCCESS) {
		/*fprintf(stderr, "Error: Unable to access %s\n", filename);*/
		return -1;
	}
	if(fileinfo.type == PR_FILE_DIRECTORY) {

		dir = PR_OpenDir(path);
		if(!dir) {
			PR_fprintf(errorFD, "Error: Unable to open directory %s.\n", path);
			errorCount++;
			return -1;
		}

		/* Recursively delete all entries in the directory */
		while((entry = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL) {
			sprintf(filename, "%s/%s", path, entry->name);
			if(rm_dash_r(filename)) return -1;
		}

		if(PR_CloseDir(dir) != PR_SUCCESS) {
			PR_fprintf(errorFD, "Error: Could not close %s.\n", path);
			errorCount++;
			return -1;
		}

		/* Delete the directory itself */
		if(PR_RmDir(path) != PR_SUCCESS) {
			PR_fprintf(errorFD, "Error: Unable to delete %s\n", path);
			errorCount++;
			return -1;
		}
	} else {
		if(PR_Delete(path) != PR_SUCCESS) {
			PR_fprintf(errorFD, "Error: Unable to delete %s\n", path);
			errorCount++;
			return -1;
		}
	}
	return 0;
}

/*
 *  u s a g e 
 * 
 *  Print some useful help information
 *
 */
void
usage (void)
{
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s %s - a signing tool for jar files\n", LONG_PROGRAM_NAME, VERSION);
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "Usage:  %s [options] directory-tree \n\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "    -b\"basename\"\t\tbasename of .sf, .rsa files for signing\n");
  PR_fprintf(outputFD, "    -c#\t\t\t\tCompression level, 0-9, 0=none\n");
  PR_fprintf(outputFD, "    -d\"certificate directory\"\tcontains cert*.db and key*.db\n");
  PR_fprintf(outputFD, "    -e\".ext\"\t\t\tsign only files with this extension\n");
  PR_fprintf(outputFD, "    -f\"filename\"\t\t\tread commands from file\n");
  PR_fprintf(outputFD, "    -G\"nickname\"\t\tcreate object-signing cert with this nickname\n");
  PR_fprintf(outputFD, "    -i\"installer script\"\tassign installer javascript\n");
  PR_fprintf(outputFD, "    -j\"javascript directory\"\tsign javascript files in this subtree\n");
  PR_fprintf(outputFD, "    -J\t\t\t\tdirectory contains HTML files. Javascript will\n"
			"\t\t\t\tbe extracted and signed.\n");
  PR_fprintf(outputFD, "    -k\"cert nickname\"\t\tsign with this certificate\n");
  PR_fprintf(outputFD, "    --leavearc\t\t\tdo not delete .arc directories created\n"
			"\t\t\t\tby -J option\n");
  PR_fprintf(outputFD, "    -m\"metafile\"\t\tinclude custom meta-information\n");
  PR_fprintf(outputFD, "    --norecurse\t\t\tdo not operate on subdirectories\n");
  PR_fprintf(outputFD, "    -o\t\t\t\toptimize - omit optional headers\n");
  PR_fprintf(outputFD, "    --outfile \"filename\"\tredirect output to file\n");
  PR_fprintf(outputFD, "    -p\"password\"\t\tfor password on command line (insecure)\n");
  PR_fprintf(outputFD, "    -s keysize\t\t\tkeysize in bits of generated cert\n");
  PR_fprintf(outputFD, "    -t token\t\t\tname of token on which to generate cert\n");
  PR_fprintf(outputFD, "    --verbosity #\t\tSet amount of debugging information to generate.\n"
			"\t\t\t\tLower number means less output, 0 is default.\n");
  PR_fprintf(outputFD, "    -x\"name\"\t\t\tdirectory or filename to exclude\n");
  PR_fprintf(outputFD, "    -z\t\t\t\tomit signing time from signature\n");
  PR_fprintf(outputFD, "    -Z\"jarfile\"\t\t\tcreate JAR file with the given name.\n"
		  "\t\t\t\t(Default compression level is 6.)\n");
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s -l\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "  lists the signing certificates in your database\n");
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s -L\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "  lists all certificates in your database, marks object-signing certificates\n");
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s -M\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "  lists the PKCS #11 modules available to %s\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s -v file.jar\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "  show the contents of the specified jar file\n");
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "%s -w file.jar\n", PROGRAM_NAME);
  PR_fprintf(outputFD, "  if valid, tries to tell you who signed the jar file\n");
  PR_fprintf(outputFD, "\n");
  PR_fprintf(outputFD, "For more details, visit\n");
  PR_fprintf(outputFD, 
"  http://developer.netscape.com/library/documentation/signedobj/signtool/\n");

  exit (ERRX);
}

/*
 *  p r i n t _ e r r o r
 *
 *  For the undocumented -E function. If an older version
 *  of communicator gives you a numeric error, we can see what
 *  really happened without doing hex math.
 *
 */

void
print_error (int err)
{
  PR_fprintf(errorFD, "Error %d: %s\n", err, JAR_get_error (err));
	errorCount++;
  give_help (err);
}

/*
 *  o u t _ o f _ m e m o r y
 *
 *  Out of memory, exit Signtool.
 * 
 */
void
out_of_memory (void)
{
  PR_fprintf(errorFD, "%s: out of memory\n", PROGRAM_NAME);
	errorCount++;
  exit (ERRX);
}

/*
 *  V e r i f y C e r t D i r
 *
 *  Validate that the specified directory
 *  contains a certificate database
 *
 */
void
VerifyCertDir(char *dir, char *keyName)
{
  char fn [FNSIZE];

  sprintf (fn, "%s/cert7.db", dir);

  if (PR_Access (fn, PR_ACCESS_EXISTS))
    {
    PR_fprintf(errorFD, "%s: No certificate database in \"%s\"\n", PROGRAM_NAME,
		dir);
    PR_fprintf(errorFD, "%s: Check the -d arguments that you gave\n",
		PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

	if(verbosity >= 0) {
		PR_fprintf(outputFD, "using certificate directory: %s\n", dir);
	}

  if (keyName == NULL)
    return;

  /* if the user gave the -k key argument, verify that 
     a key database already exists */

  sprintf (fn, "%s/key3.db", dir);

  if (PR_Access (fn, PR_ACCESS_EXISTS))
    {
    PR_fprintf(errorFD, "%s: No private key database in \"%s\"\n", PROGRAM_NAME,
		dir);
    PR_fprintf(errorFD, "%s: Check the -d arguments that you gave\n",
		PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }
}

/*
 *  f o r e a c h 
 * 
 *  A recursive function to loop through all names in
 *  the specified directory, as well as all subdirectories.
 *
 *  FIX: Need to see if all platforms allow multiple
 *  opendir's to be called.
 *
 */

int
foreach(char *dirname, char *prefix, 
       int (*fn)(char *relpath, char *basedir, char *reldir, char *filename,
		void* arg),
		PRBool recurse, PRBool includeDirs, void *arg) {
	char newdir [FNSIZE];
	int retval = 0;

	PRDir *dir;
	PRDirEntry *entry;

	strcpy (newdir, dirname);
	if (*prefix) {
		strcat (newdir, "/");
		strcat (newdir, prefix);
	}

	dir = PR_OpenDir (newdir);
	if (!dir) return -1;

	for (entry = PR_ReadDir (dir,0); entry; entry = PR_ReadDir (dir,0)) {
		if ( strcmp(entry->name, ".")==0   ||
                     strcmp(entry->name, "..")==0 )
                {
                    /* no infinite recursion, please */   
		    continue;
                }

		/* can't sign self */
		if (!strcmp (entry->name, "META-INF"))
			continue;

		/* -x option */
		if (PL_HashTableLookup(excludeDirs, entry->name))
			continue;

		strcpy (newdir, dirname);
		if (*dirname)
			strcat (newdir, "/");

		if (*prefix) {
			strcat (newdir, prefix);
			strcat (newdir, "/");
		}
		strcat (newdir, entry->name);

		if(!is_dir(newdir) || includeDirs) {
			char newpath [FNSIZE];

			strcpy (newpath, prefix);
			if (*newpath)
				strcat (newpath, "/");
			strcat (newpath, entry->name);

			if( (*fn) (newpath, dirname, prefix, (char *) entry->name, arg)) {
				retval = -1;
				break;
			}
		}

		if (is_dir (newdir)) {
			if(recurse) {
				char newprefix [FNSIZE];

				strcpy (newprefix, prefix);
				if (*newprefix) {
					strcat (newprefix, "/");
				}
				strcat (newprefix, entry->name);

				if(foreach (dirname, newprefix, fn, recurse, includeDirs,arg)) {
					retval = -1;
					break;
				}
			}
		}

	}

	PR_CloseDir (dir);

	return retval;
}

/*
 *  i s _ d i r
 *
 *  Return 1 if file is a directory.
 *  Wonder if this runs on a mac, trust not.
 *
 */
static int is_dir (char *filename)
{
	PRFileInfo	finfo;

	if( PR_GetFileInfo(filename, &finfo) != PR_SUCCESS ) {
		printf("Unable to get information about %s\n", filename);
		return 0;
	}

	return ( finfo.type == PR_FILE_DIRECTORY );
}

/*
 *  p a s s w o r d _ h a r d c o d e 
 *
 *  A function to use the password passed in the -p(password) argument
 *  of the command line. This is only to be used for build & testing purposes,
 *  as it's extraordinarily insecure. 
 *
 *  After use once, null it out otherwise PKCS11 calls us forever.
 *
 */
SECItem *
password_hardcode(void *arg, SECKEYKeyDBHandle *handle)
{
  SECItem *pw = NULL;
  if (password) {
    pw = SECITEM_AllocItem(NULL, NULL, PL_strlen(password));
    pw->data = PL_strdup(password);
    password = NULL;
  }
  return pw;
}

char *
pk11_password_hardcode(PK11SlotInfo *slot, PRBool retry, void *arg)
{
  char *pw;
  pw = password ? PORT_Strdup (password) : NULL;
  password = NULL;
  return pw;
}

/************************************************************************
 *
 * c e r t D B N a m e C a l l b a c k
 */
static char *
certDBNameCallback(void *arg, int dbVersion)
{
    char *fnarg;
    char *dir;
    char *filename;
    
    dir = SECU_ConfigDirectory (NULL);

    switch ( dbVersion ) {
      case 7:
        fnarg = "7";
        break;
      case 6:
	fnarg = "6";
	break;
      case 5:
	fnarg = "5";
	break;
      case 4:
      default:
	fnarg = "";
	break;
    }
    filename = PR_smprintf("%s/cert%s.db", dir, fnarg);
    return(filename);
}

/***************************************************************
 *
 * s e c E r r o r S t r i n g
 *
 * Returns an error string corresponding to the given error code.
 * Doesn't cover all errors; returns a default for many.
 * Returned string is only valid until the next call of this function.
 */
const char*
secErrorString(long code)
{
	static char errstring[80]; /* dynamically constructed error string */
	char *c; /* the returned string */

	switch(code) {
	case SEC_ERROR_IO: c = "io error";
		break;
	case SEC_ERROR_LIBRARY_FAILURE: c = "security library failure";
		break;
	case SEC_ERROR_BAD_DATA: c = "bad data";
		break;
	case SEC_ERROR_OUTPUT_LEN: c = "output length";
		break;
	case SEC_ERROR_INPUT_LEN: c = "input length";
		break;
	case SEC_ERROR_INVALID_ARGS: c = "invalid args";
		break;
	case SEC_ERROR_EXPIRED_CERTIFICATE: c = "expired certificate";
		break;
	case SEC_ERROR_REVOKED_CERTIFICATE: c = "revoked certificate";
		break;
	case SEC_ERROR_INADEQUATE_KEY_USAGE: c = "inadequate key usage";
		break;
	case SEC_ERROR_INADEQUATE_CERT_TYPE: c = "inadequate certificate type";
		break;
	case SEC_ERROR_UNTRUSTED_CERT: c = "untrusted cert";
		break;
	case SEC_ERROR_NO_KRL: c = "no key revocation list";
		break;
	case SEC_ERROR_KRL_BAD_SIGNATURE: c = "key revocation list: bad signature";
		break;
	case SEC_ERROR_KRL_EXPIRED: c = "key revocation list expired";
		break;
	case SEC_ERROR_REVOKED_KEY: c = "revoked key";
		break;
	case SEC_ERROR_CRL_BAD_SIGNATURE:
		c = "certificate revocation list: bad signature";
		break;
	case SEC_ERROR_CRL_EXPIRED: c = "certificate revocation list expired";
		break;
	case SEC_ERROR_CRL_NOT_YET_VALID:
		c = "certificate revocation list not yet valid";
		break;
	case SEC_ERROR_UNKNOWN_ISSUER: c = "unknown issuer";
		break;
	case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE: c = "expired issuer certificate";
		break;
	case SEC_ERROR_BAD_SIGNATURE: c = "bad signature";
		break;
	case SEC_ERROR_BAD_KEY: c = "bad key";
		break;
	case SEC_ERROR_NOT_FORTEZZA_ISSUER: c = "not fortezza issuer";
		break;
	case SEC_ERROR_CA_CERT_INVALID:
		c = "Certificate Authority certificate invalid";
		break;
	case SEC_ERROR_EXTENSION_NOT_FOUND: c = "extension not found";
		break;
	case SEC_ERROR_CERT_NOT_IN_NAME_SPACE: c = "certificate not in name space";
		break;
	case SEC_ERROR_UNTRUSTED_ISSUER: c = "untrusted issuer";
		break;
	default:
		sprintf(errstring, "security error %ld", code);
		c = errstring;
		break;
	}

	return c;
}

/***************************************************************
 *
 * d i s p l a y V e r i f y L o g
 *
 * Prints the log of a cert verification.
 */
void
displayVerifyLog(CERTVerifyLog *log)
{
	CERTVerifyLogNode	*node;
	CERTCertificate		*cert;
	char				*name;

	if( !log  || (log->count <= 0) ) {
		return;
	}

	for(node = log->head; node != NULL; node = node->next) {

		if( !(cert = node->cert) ) {
			continue;
		}

		/* Get a name for this cert */
		if(cert->nickname != NULL) {
			name = cert->nickname;
		} else if(cert->emailAddr != NULL) {
			name = cert->emailAddr;
		} else {
			name = cert->subjectName;
		}

		printf( "%s%s:\n",
			name,
			(node->depth > 0) ? " [Certificate Authority]" : ""
		);

		printf("\t%s\n", secErrorString(node->error));

	}
}
/*
 *  J a r L i s t M o d u l e s
 *
 *  Print a list of the PKCS11 modules that are
 *  available. This is useful for smartcard people to
 *  make sure they have the drivers loaded.
 *
 */
void
JarListModules(void)
{
  int i;
  int count = 0;

  SECMODModuleList *modules = NULL;
  static SECMODListLock *moduleLock = NULL;

  SECMODModuleList *mlp;

  modules = SECMOD_GetDefaultModuleList();

  if (modules == NULL)
    {
    PR_fprintf(errorFD, "%s: Can't get module list\n", PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

  if ((moduleLock = SECMOD_NewListLock()) == NULL)
    {
    /* this is the wrong text */
    PR_fprintf(errorFD, "%s: unable to acquire lock on module list\n",
		PROGRAM_NAME);
	errorCount++;
    exit (ERRX);
    }

  SECMOD_GetReadLock (moduleLock);

  PR_fprintf(outputFD, "\nListing of PKCS11 modules\n");
  PR_fprintf(outputFD, "-----------------------------------------------\n");
 
  for (mlp = modules; mlp != NULL; mlp = mlp->next) 
    {
    count++;
    PR_fprintf(outputFD, "%3d. %s\n", count, mlp->module->commonName);

    if (mlp->module->internal)
      PR_fprintf(outputFD, "          (this module is internally loaded)\n");
    else
      PR_fprintf(outputFD, "          (this is an external module)\n");

    if (mlp->module->dllName)
      PR_fprintf(outputFD, "          DLL name: %s\n", mlp->module->dllName);

    if (mlp->module->slotCount == 0)
      PR_fprintf(outputFD, "          slots: There are no slots attached to this module\n");
    else
      PR_fprintf(outputFD, "          slots: %d slots attached\n", mlp->module->slotCount);

    if (mlp->module->loaded == 0)
      PR_fprintf(outputFD, "          status: Not loaded\n");
    else
      PR_fprintf(outputFD, "          status: loaded\n");

    for (i = 0; i < mlp->module->slotCount; i++) 
      {
      PK11SlotInfo *slot = mlp->module->slots[i];

      PR_fprintf(outputFD, "\n");
      PR_fprintf(outputFD, "    slot: %s\n", PK11_GetSlotName(slot));
      PR_fprintf(outputFD, "   token: %s\n", PK11_GetTokenName(slot));
      }
    }

  PR_fprintf(outputFD, "-----------------------------------------------\n");

  if (count == 0)
    PR_fprintf(outputFD,
		"Warning: no modules were found (should have at least one)\n");

  SECMOD_ReleaseReadLock (moduleLock);
}

/**********************************************************************
 * c h o p
 *
 * Eliminates leading and trailing whitespace.  Returns a pointer to the 
 * beginning of non-whitespace, or an empty string if it's all whitespace.
 */
char*
chop(char *str)
{
	char *start, *end;

	if(str) {
		start = str;

		/* Nip leading whitespace */
		while(isspace(*start)) {
			start++;
		}

		/* Nip trailing whitespace */
		if(strlen(start) > 0) {
			end = start + strlen(start) - 1;
			while(isspace(*end) && end > start) {
				end--;
			}
			*(end+1) = '\0';
		}
		
		return start;
	} else {
		return NULL;
	}
}

/***********************************************************************
 *
 * F a t a l E r r o r
 *
 * Outputs an error message and bails out of the program.
 */
void
FatalError(char *msg)
{
	if(!msg) msg = "";

	PR_fprintf(errorFD, "FATAL ERROR: %s\n", msg);
	errorCount++;
	exit(ERRX);
}

/*************************************************************************
 *
 * I n i t C r y p t o
 */
int
InitCrypto(char *cert_dir, PRBool readOnly)
{
  SECStatus rv;
  static int prior = 0;
	PK11SlotInfo *slotinfo;

  CERTCertDBHandle *db;

	if (prior == 0) {
		/* some functions such as OpenKeyDB expect this path to be
		 * implicitly set prior to calling */
		if (readOnly) {
		    rv = NSS_Init(cert_dir);
		} else {
		    rv = NSS_InitReadWrite(cert_dir);
		}
    		if (rv != SECSuccess) {
		    SECU_PrintPRandOSError(PROGRAM_NAME);
		    exit(-1);
    		}

		SECU_ConfigDirectory (cert_dir);

		/* Been there done that */
		prior++;

		if(password) {
			PK11_SetPasswordFunc(pk11_password_hardcode);
		} 

		/* Must login to FIPS before you do anything else */
		if(PK11_IsFIPS()) {
			slotinfo = PK11_GetInternalSlot();
			if(!slotinfo) {
				fprintf(stderr, "%s: Unable to get PKCS #11 Internal Slot."
				  "\n", PROGRAM_NAME);
				return -1;
			}
			if(PK11_Authenticate(slotinfo, PR_FALSE /*loadCerts*/,
			  NULL /*wincx*/) != SECSuccess) {
				fprintf(stderr, "%s: Unable to authenticate to %s.\n",
					PROGRAM_NAME, PK11_GetSlotName(slotinfo));
				return -1;
			}
		}

		/* Make sure there is a password set on the internal key slot */
		slotinfo = PK11_GetInternalKeySlot();
		if(!slotinfo) {
			fprintf(stderr, "%s: Unable to get PKCS #11 Internal Key Slot."
			  "\n", PROGRAM_NAME);
			return -1;
		}
		if(PK11_NeedUserInit(slotinfo)) {
			PR_fprintf(errorFD,
"\nWARNING: No password set on internal key database.  Most operations will fail."
"\nYou must use Communicator to create a password.\n");
			warningCount++;
		}

		/* Make sure we can authenticate to the key slot in FIPS mode */
		if(PK11_IsFIPS()) {
			if(PK11_Authenticate(slotinfo, PR_FALSE /*loadCerts*/,
			  NULL /*wincx*/) != SECSuccess) {
				fprintf(stderr, "%s: Unable to authenticate to %s.\n",
					PROGRAM_NAME, PK11_GetSlotName(slotinfo));
				return -1;
			}
		}
	}

	return 0;
}

/* Windows foolishness is now in the secutil lib */

/*****************************************************************
 *  g e t _ d e f a u l t _ c e r t _ d i r
 *
 *  Attempt to locate a certificate directory.
 *  Failing that, complain that the user needs to
 *  use the -d(irectory) parameter.
 *
 */
char *get_default_cert_dir (void)
{
  char *home;

  char *cd = NULL;
  static char db [FNSIZE];

#ifdef XP_UNIX
  home = getenv ("HOME");

  if (home && *home)
    {
    sprintf (db, "%s/.netscape", home);
    cd = db;
    }
#endif

#ifdef XP_PC
  FILE *fp;

  /* first check the environment override */

  home = getenv ("JAR_HOME");

  if (home && *home)
    {
    sprintf (db, "%s/cert7.db", home);

    if ((fp = fopen (db, "r")) != NULL)
      {
      fclose (fp);
      cd = home;
      }
    }

  /* try the old navigator directory */

  if (cd == NULL)
    {
    home = "c:/Program Files/Netscape/Navigator";

    sprintf (db, "%s/cert7.db", home);

    if ((fp = fopen (db, "r")) != NULL)
      {
      fclose (fp);
      cd = home;
      }
    }

  /* Try the current directory, I wonder if this
     is really a good idea. Remember, Windows only.. */

  if (cd == NULL)
    {
    home = ".";

    sprintf (db, "%s/cert7.db", home);

    if ((fp = fopen (db, "r")) != NULL)
      {
      fclose (fp);
      cd = home;
      }
    }

#endif

  if (!cd)
    {
    PR_fprintf(errorFD,
		"You must specify the location of your certificate directory\n");
    PR_fprintf(errorFD,
		"with the -d option. Example: -d ~/.netscape in many cases with Unix.\n");
	errorCount++;
    exit (ERRX);
    }

  return cd;
}

/************************************************************************
 * g i v e _ h e l p
 */
void give_help (int status)
{
  if (status == SEC_ERROR_UNKNOWN_ISSUER)
    {
    PR_fprintf(errorFD,
		"The Certificate Authority (CA) for this certificate\n");
    PR_fprintf(errorFD,
		"does not appear to be in your database. You should contact\n");
    PR_fprintf(errorFD,
		"the organization which issued this certificate to obtain\n");
    PR_fprintf(errorFD, "a copy of its CA Certificate.\n");
    }
}

/**************************************************************************
 *
 * p r _ f g e t s
 *
 * fgets implemented with NSPR.
 */
char*
pr_fgets(char *buf, int size, PRFileDesc *file)
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

