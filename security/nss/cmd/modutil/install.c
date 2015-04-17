/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "install.h"
#include "install-ds.h"
#include <prerror.h>
#include <prlock.h>
#include <prio.h>
#include <prmem.h>
#include <prprf.h>
#include <prsystem.h>
#include <prproces.h>

#ifdef XP_UNIX
/* for chmod */
#include <sys/types.h>
#include <sys/stat.h>
#endif

/*extern "C" {*/
#include <jar.h>
/*}*/

extern /*"C"*/
int Pk11Install_AddNewModule(char* moduleName, char* dllPath,
                              unsigned long defaultMechanismFlags,
                              unsigned long cipherEnableFlags);
extern /*"C"*/
short Pk11Install_UserVerifyJar(JAR *jar, PRFileDesc *out,
	PRBool query);
extern /*"C"*/
const char* mySECU_ErrorString(PRErrorCode errnum);
extern 
int Pk11Install_yyparse();

#define INSTALL_METAINFO_TAG "Pkcs11_install_script"
#define SCRIPT_TEMP_FILE "pkcs11inst.tmp"
#define ROOT_MARKER "%root%"
#define TEMP_MARKER "%temp%"
#define PRINTF_ROOT_MARKER "%%root%%"
#define TEMPORARY_DIRECTORY_NAME "pk11inst.dir"
#define JAR_BASE_END (JAR_BASE+100)

static PRLock* errorHandlerLock=NULL;
static Pk11Install_ErrorHandler errorHandler=NULL;
static char* PR_Strdup(const char* str);
static int rm_dash_r (char *path);
static int make_dirs(char *path, int file_perms);
static int dir_perms(int perms);

static Pk11Install_Error DoInstall(JAR *jar, const char *installDir,
	const char* tempDir, Pk11Install_Platform *platform, 
	PRFileDesc *feedback, PRBool noverify);

static char *errorString[]= {
	"Operation was successful", /* PK11_INSTALL_NO_ERROR */
	"Directory \"%s\" does not exist", /* PK11_INSTALL_DIR_DOESNT_EXIST */
	"File \"%s\" does not exist", /* PK11_INSTALL_FILE_DOESNT_EXIST */
	"File \"%s\" is not readable", /* PK11_INSTALL_FILE_NOT_READABLE */
	"%s",		/* PK11_INSTALL_ERROR_STRING */
	"Error in JAR file %s: %s",  /* PK11_INSTALL_JAR_ERROR */
	"No Pkcs11_install_script specified in JAR metainfo file",
				/* PK11_INSTALL_NO_INSTALLER_SCRIPT */
	"Could not delete temporary file \"%s\"",
				/*PK11_INSTALL_DELETE_TEMP_FILE */
	"Could not open temporary file \"%s\"", /*PK11_INSTALL_OPEN_SCRIPT_FILE*/
	"%s: %s", /* PK11_INSTALL_SCRIPT_PARSE */
	"Error in script: %s",
	"Unable to obtain system platform information",
	"Installer script has no information about the current platform (%s)",
	"Relative directory \"%s\" does not contain "PRINTF_ROOT_MARKER,
	"Module File \"%s\" not found",
	"Error occurred installing module \"%s\" into database",
	"Error extracting \"%s\" from JAR file: %s",
	"Directory \"%s\" is not writeable",
	"Could not create directory \"%s\"",
	"Could not remove directory \"%s\"",
	"Unable to execute \"%s\"",
	"Unable to wait for process \"%s\"",
	"\"%s\" returned error code %d",
	"User aborted operation",
	"Unspecified error"
};

enum {
	INSTALLED_FILE_MSG=0,
	INSTALLED_MODULE_MSG,
	INSTALLER_SCRIPT_NAME,
	MY_PLATFORM_IS,
	USING_PLATFORM,
	PARSED_INSTALL_SCRIPT,
	EXEC_FILE_MSG,
	EXEC_SUCCESS,
	INSTALLATION_COMPLETE_MSG,
	USER_ABORT
};

static char *msgStrings[] = {
	"Installed file %s to %s\n",
	"Installed module \"%s\" into module database\n",
	"Using installer script \"%s\"\n",
	"Current platform is %s\n",
	"Using installation parameters for platform %s\n",
	"Successfully parsed installation script\n",
	"Executing \"%s\"...\n",
	"\"%s\" executed successfully\n",
	"\nInstallation completed successfully\n",
	"\nAborting...\n"
};

/**************************************************************************
 * S t r i n g N o d e
 */
typedef struct StringNode_str {
    char *str;
    struct StringNode_str* next;
} StringNode;

StringNode* StringNode_new()
{
	StringNode* new_this;
	new_this = (StringNode*)PR_Malloc(sizeof(StringNode));
	PORT_Assert(new_this != NULL);
	new_this->str = NULL;
	new_this->next = NULL;
	return new_this;
}

void StringNode_delete(StringNode* s) 
{
	if(s->str) {
		PR_Free(s->str);
		s->str=NULL;
	}
}

/*************************************************************************
 * S t r i n g L i s t
 */
typedef struct StringList_str {
  StringNode* head;
	StringNode* tail;
} StringList;

void StringList_new(StringList* list)
{
  list->head=NULL;
  list->tail=NULL;
}

void StringList_delete(StringList* list)
{
	StringNode *tmp;
	while(list->head) {
		tmp = list->head;
		list->head = list->head->next;
		StringNode_delete(tmp);
	}
}

void
StringList_Append(StringList* list, char* str)
{
	if(!str) {
		return;
	}

	if(!list->tail) {
		/* This is the first element */
	  list->head = list->tail = StringNode_new();
	} else {
		list->tail->next = StringNode_new();
		list->tail = list->tail->next;
	}

	list->tail->str = PR_Strdup(str);
	list->tail->next = NULL;	/* just to be sure */
}

/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ S e t E r r o r H a n d l e r
 *
 * Sets the error handler to be used by the library.  Returns the current
 * error handler function.
 */
Pk11Install_ErrorHandler
Pk11Install_SetErrorHandler(Pk11Install_ErrorHandler handler)
{
	Pk11Install_ErrorHandler old;

	if(!errorHandlerLock) {
		errorHandlerLock = PR_NewLock();
	}

	PR_Lock(errorHandlerLock);

	old = errorHandler;
	errorHandler = handler;

	PR_Unlock(errorHandlerLock);

	return old;
}

/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ I n i t
 *
 * Does initialization that otherwise would be done on the fly.  Only
 * needs to be called by multithreaded apps, before they make any calls
 * to this library.
 */
void
Pk11Install_Init()
{
	if(!errorHandlerLock) {
		errorHandlerLock = PR_NewLock();
	}
}

/**************************************************************************
 *
 * P k 1 1 I n s t a l l _ R e l e a s e
 *
 * Releases static data structures used by the library.  Don't use the
 * library after calling this, unless you call Pk11Install_Init()
 * first.  This function doesn't have to be called at all unless you're
 * really anal about freeing memory before your program exits.
 */
void
Pk11Install_Release()
{
	if(errorHandlerLock) {
		PR_Free(errorHandlerLock);
		errorHandlerLock = NULL;
	}
}

/*************************************************************************
 *
 * e r r o r
 *
 * Takes an error code and its arguments, creates the error string,
 * and sends the string to the handler function if it exists.
 */

#ifdef OSF1
/* stdarg has already been pulled in from NSPR */
#undef va_start
#undef va_end
#undef va_arg
#include <varargs.h>
#else
#include <stdarg.h>
#endif

#ifdef OSF1
static void
error(long va_alist, ...)
#else
static void
error(Pk11Install_Error errcode, ...)
#endif
{

	va_list ap;
	char *errstr;
	Pk11Install_ErrorHandler handler;

	if(!errorHandlerLock) {
		errorHandlerLock = PR_NewLock();
	}

	PR_Lock(errorHandlerLock);

	handler = errorHandler;

	PR_Unlock(errorHandlerLock);

	if(handler) {
#ifdef OSF1
		va_start(ap);
		errstr = PR_vsmprintf(errorString[va_arg(ap, Pk11Install_Error)], ap);
#else
		va_start(ap, errcode);
		errstr = PR_vsmprintf(errorString[errcode], ap);
#endif
		handler(errstr);
		PR_smprintf_free(errstr);
		va_end(ap);
	}
}

/*************************************************************************
 *
 * j a r _ c a l l b a c k
 */
static int
jar_callback(int status, JAR *foo, const char *bar, char *pathname,
	char *errortext) {
	char *string;

	string = PR_smprintf("JAR error %d: %s in file %s\n", status, errortext,
		pathname);
	error(PK11_INSTALL_ERROR_STRING, string);
	PR_smprintf_free(string);
	return 0;
}
	
/*************************************************************************
 *
 * P k 1 1 I n s t a l l _ D o I n s t a l l
 *
 * jarFile is the path of a JAR in the PKCS #11 module JAR format.
 * installDir is the directory relative to which files will be
 *   installed.
 */
Pk11Install_Error
Pk11Install_DoInstall(char *jarFile, const char *installDir,
	const char *tempDir, PRFileDesc *feedback, short force, PRBool noverify)
{
	JAR *jar;
	char *installer;
	unsigned long installer_len;
	int status;
	Pk11Install_Error ret;
	PRBool made_temp_file;
	Pk11Install_Info installInfo;
	Pk11Install_Platform *platform;
	char* errMsg;
	char sysname[SYS_INFO_BUFFER_LENGTH], release[SYS_INFO_BUFFER_LENGTH],
       arch[SYS_INFO_BUFFER_LENGTH];
	char *myPlatform;

	jar=NULL;
	ret = PK11_INSTALL_UNSPECIFIED;
	made_temp_file=PR_FALSE;
	errMsg=NULL;
	Pk11Install_Info_init(&installInfo);

	/*
	printf("Inside DoInstall, jarFile=%s, installDir=%s, tempDir=%s\n",
		jarFile, installDir, tempDir);
	*/

	/*
	 * Check out jarFile and installDir for validity
	 */
	if( PR_Access(installDir, PR_ACCESS_EXISTS) != PR_SUCCESS ) {
		error(PK11_INSTALL_DIR_DOESNT_EXIST, installDir);
		return PK11_INSTALL_DIR_DOESNT_EXIST;
	}
	if(!tempDir) {
		tempDir = ".";
	}
	if( PR_Access(tempDir, PR_ACCESS_EXISTS) != PR_SUCCESS ) {
		error(PK11_INSTALL_DIR_DOESNT_EXIST, tempDir);
		return PK11_INSTALL_DIR_DOESNT_EXIST;
	}
	if( PR_Access(tempDir, PR_ACCESS_WRITE_OK) != PR_SUCCESS ) {
		error(PK11_INSTALL_DIR_NOT_WRITEABLE, tempDir);
		return PK11_INSTALL_DIR_NOT_WRITEABLE;
	}
	if( (PR_Access(jarFile, PR_ACCESS_EXISTS) != PR_SUCCESS) ) {
		error(PK11_INSTALL_FILE_DOESNT_EXIST, jarFile);
		return PK11_INSTALL_FILE_DOESNT_EXIST;
	}
	if( PR_Access(jarFile, PR_ACCESS_READ_OK) != PR_SUCCESS ) {
		error(PK11_INSTALL_FILE_NOT_READABLE, jarFile);
		return PK11_INSTALL_FILE_NOT_READABLE;
	}

	/*
	 * Extract the JAR file
	 */
	jar = JAR_new();
	JAR_set_callback(JAR_CB_SIGNAL, jar, jar_callback);

	if(noverify) {
		status = JAR_pass_archive_unverified(jar, jarArchGuess, jarFile, "url");
	} else {
		status = JAR_pass_archive(jar, jarArchGuess, jarFile, "url");
	}
	if( (status < 0) || (jar->valid < 0) ) {
		if (status >= JAR_BASE && status <= JAR_BASE_END) {
			error(PK11_INSTALL_JAR_ERROR, jarFile, JAR_get_error(status));
		} else {
			error(PK11_INSTALL_JAR_ERROR, jarFile,
			  mySECU_ErrorString(PORT_GetError()));
		}
		ret=PK11_INSTALL_JAR_ERROR;
		goto loser;
	}
	/*printf("passed the archive\n");*/

	/*
	 * Show the user security information, allow them to abort or continue
	 */
	if( Pk11Install_UserVerifyJar(jar, PR_STDOUT,
		force?PR_FALSE:PR_TRUE) && !force) {
		if(feedback) {
			PR_fprintf(feedback, msgStrings[USER_ABORT]);
		}
		ret=PK11_INSTALL_USER_ABORT;
		goto loser;
	}

	/*
	 * Get the name of the installation file
	 */
	if( JAR_get_metainfo(jar, NULL, INSTALL_METAINFO_TAG, (void**)&installer,
		(unsigned long*)&installer_len) ) {
		error(PK11_INSTALL_NO_INSTALLER_SCRIPT);
		ret=PK11_INSTALL_NO_INSTALLER_SCRIPT;
		goto loser;
	}
	if(feedback) {
		PR_fprintf(feedback, msgStrings[INSTALLER_SCRIPT_NAME], installer);
	}

	/*
	 * Extract the installation file
	 */
	if( PR_Access(SCRIPT_TEMP_FILE, PR_ACCESS_EXISTS) == PR_SUCCESS) {
		if( PR_Delete(SCRIPT_TEMP_FILE) != PR_SUCCESS) {
			error(PK11_INSTALL_DELETE_TEMP_FILE, SCRIPT_TEMP_FILE);
			ret=PK11_INSTALL_DELETE_TEMP_FILE;
			goto loser;
		}
	}
	if(noverify) {
		status = JAR_extract(jar, installer, SCRIPT_TEMP_FILE);
	} else {
		status = JAR_verified_extract(jar, installer, SCRIPT_TEMP_FILE);
	}
	if(status) {
		if (status >= JAR_BASE && status <= JAR_BASE_END) {
			error(PK11_INSTALL_JAR_EXTRACT, installer, JAR_get_error(status));
		} else {
			error(PK11_INSTALL_JAR_EXTRACT, installer,
			  mySECU_ErrorString(PORT_GetError()));
		}
		ret = PK11_INSTALL_JAR_EXTRACT;
		goto loser;
	} else {
		made_temp_file = PR_TRUE;
	}

	/*
	 * Parse the installation file into a syntax tree
	 */
	Pk11Install_FD = PR_Open(SCRIPT_TEMP_FILE, PR_RDONLY, 0);
	if(!Pk11Install_FD) {
		error(PK11_INSTALL_OPEN_SCRIPT_FILE, SCRIPT_TEMP_FILE);
		ret=PK11_INSTALL_OPEN_SCRIPT_FILE;
		goto loser;
	}
	if(Pk11Install_yyparse()) {
		error(PK11_INSTALL_SCRIPT_PARSE, installer,
			Pk11Install_yyerrstr ? Pk11Install_yyerrstr : "");
		ret=PK11_INSTALL_SCRIPT_PARSE;
		goto loser;
	}

#if 0
	/* for debugging */
	Pk11Install_valueList->Print(0);
#endif

	/*
	 * From the syntax tree, build a semantic structure
	 */
	errMsg = Pk11Install_Info_Generate(&installInfo,Pk11Install_valueList);
	if(errMsg) {
		error(PK11_INSTALL_SEMANTIC, errMsg);
		ret=PK11_INSTALL_SEMANTIC;
		goto loser;
	}
#if 0
	installInfo.Print(0);
#endif

	if(feedback) {
		PR_fprintf(feedback, msgStrings[PARSED_INSTALL_SCRIPT]);
	}

	/*
	 * Figure out which platform to use
	 */
	{
		sysname[0] = release[0] = arch[0] = '\0';

		if( (PR_GetSystemInfo(PR_SI_SYSNAME, sysname, SYS_INFO_BUFFER_LENGTH)
				!= PR_SUCCESS) ||
		    (PR_GetSystemInfo(PR_SI_RELEASE, release, SYS_INFO_BUFFER_LENGTH)
				!= PR_SUCCESS) ||
		    (PR_GetSystemInfo(PR_SI_ARCHITECTURE, arch, SYS_INFO_BUFFER_LENGTH)
				!= PR_SUCCESS) ) {
			error(PK11_INSTALL_SYSINFO);
			ret=PK11_INSTALL_SYSINFO;
			goto loser;
		}
		myPlatform = PR_smprintf("%s:%s:%s", sysname, release, arch);
		platform = Pk11Install_Info_GetBestPlatform(&installInfo,myPlatform);
		if(!platform) {
			error(PK11_INSTALL_NO_PLATFORM, myPlatform);
			PR_smprintf_free(myPlatform);
			ret=PK11_INSTALL_NO_PLATFORM;
			goto loser;
		}
		if(feedback) {
			PR_fprintf(feedback, msgStrings[MY_PLATFORM_IS], myPlatform);
			PR_fprintf(feedback, msgStrings[USING_PLATFORM],
                    Pk11Install_PlatformName_GetString(&platform->name));
		}
		PR_smprintf_free(myPlatform);
	}

	/* Run the install for that platform */
	ret = DoInstall(jar, installDir, tempDir, platform, feedback, noverify);
	if(ret) {
		goto loser;
	}

	ret = PK11_INSTALL_SUCCESS;
loser:
	if(Pk11Install_valueList) {
		Pk11Install_ValueList_delete(Pk11Install_valueList);
		PR_Free(Pk11Install_valueList);
		Pk11Install_valueList = NULL;
	}
	if(jar) {
		JAR_destroy(jar);
	}
	if(made_temp_file) {
		PR_Delete(SCRIPT_TEMP_FILE);
	}
	if(errMsg) {
		PR_smprintf_free(errMsg);
	}
	return ret;
}

/*
/////////////////////////////////////////////////////////////////////////
// actually run the installation, copying files to and fro
*/
static Pk11Install_Error
DoInstall(JAR *jar, const char *installDir, const char *tempDir,
	Pk11Install_Platform *platform, PRFileDesc *feedback, PRBool noverify)
{
	Pk11Install_File *file;
	Pk11Install_Error ret;
	char *reldir;
	char *dest;
	char *modDest;
	char *cp;
	int i;
	int status;
	char *tempname, *temp;
	StringList executables;
	StringNode *execNode;
	PRProcessAttr *attr;
	PRProcess *proc;
	char *argv[2];
	char *envp[1];
	int errcode;

	ret=PK11_INSTALL_UNSPECIFIED;
	reldir=NULL;
	dest=NULL;
	modDest=NULL;
	tempname=NULL;

	StringList_new(&executables);
	/*
	// Create Temporary directory
	*/
	tempname = PR_smprintf("%s/%s", tempDir, TEMPORARY_DIRECTORY_NAME);
	if( PR_Access(tempname, PR_ACCESS_EXISTS)==PR_SUCCESS ) {
		/* Left over from previous run?  Delete it. */
		rm_dash_r(tempname);
	}
	if(PR_MkDir(tempname, 0700) != PR_SUCCESS) {
		error(PK11_INSTALL_CREATE_DIR, tempname);
		ret = PK11_INSTALL_CREATE_DIR;
		goto loser;
	}

	/*
	// Install all the files
	*/
	for(i=0; i < platform->numFiles; i++) {
		file = &platform->files[i];

		if(file->relativePath) {
			PRBool foundMarker = PR_FALSE;
			reldir = PR_Strdup(file->relativePath);

			/* Replace all the markers with the directories for which they stand */
			while(1) {
				if( (cp=PL_strcasestr(reldir, ROOT_MARKER)) ) {
					/* Has a %root% marker  */
					*cp = '\0';
					temp = PR_smprintf("%s%s%s", reldir, installDir,
						cp+strlen(ROOT_MARKER));
					PR_Free(reldir);
					reldir = temp;
					foundMarker = PR_TRUE;
				} else if( (cp = PL_strcasestr(reldir, TEMP_MARKER)) ) {
					/* Has a %temp% marker */
					*cp = '\0';
					temp = PR_smprintf("%s%s%s", reldir, tempname, 
						cp+strlen(TEMP_MARKER));
					PR_Free(reldir);
					reldir = temp;
					foundMarker = PR_TRUE;
				} else {
					break;
				}
			}
			if(!foundMarker) {
				/* Has no markers...this isn't really a relative directory */
				error(PK11_INSTALL_BOGUS_REL_DIR, file->relativePath);
				ret = PK11_INSTALL_BOGUS_REL_DIR;
				goto loser;
			}
			dest = reldir;
			reldir = NULL;
		} else if(file->absolutePath) {
			dest = PR_Strdup(file->absolutePath);
		}

		/* Remember if this is the module file, we'll need to add it later */
		if(i == platform->modFile) {
			modDest = PR_Strdup(dest);
		}

		/* Remember is this is an executable, we'll need to run it later */
		if(file->executable) {
			StringList_Append(&executables,dest);
			/*executables.Append(dest);*/
		}

		/* Make sure the directory we are targetting exists */
		if( make_dirs(dest, file->permissions) ) {
			ret=PK11_INSTALL_CREATE_DIR;
			goto loser;
		}

		/* Actually extract the file onto the filesystem */
		if(noverify) {
			status = JAR_extract(jar, (char*)file->jarPath, dest);
		} else {
			status = JAR_verified_extract(jar, (char*)file->jarPath, dest);
		}
		if(status) {
			if (status >= JAR_BASE && status <= JAR_BASE_END) {
				error(PK11_INSTALL_JAR_EXTRACT, file->jarPath,
                  JAR_get_error(status));
			} else {
				error(PK11_INSTALL_JAR_EXTRACT, file->jarPath,
				  mySECU_ErrorString(PORT_GetError()));
			}
			ret=PK11_INSTALL_JAR_EXTRACT;
			goto loser;
		}
		if(feedback) {
			PR_fprintf(feedback, msgStrings[INSTALLED_FILE_MSG],
				file->jarPath, dest);
		}

		/* no NSPR command to change permissions? */
#ifdef XP_UNIX
		chmod(dest, file->permissions);
#endif

		/* Memory clean-up tasks */
		if(reldir) {
			PR_Free(reldir);
			reldir = NULL;
		}
		if(dest) {
			PR_Free(dest);
			dest = NULL;
		}
	}
	/* Make sure we found the module file */
	if(!modDest) {
		/* Internal problem here, since every platform is supposed to have
		   a module file */
		error(PK11_INSTALL_NO_MOD_FILE, platform->moduleName);
		ret=PK11_INSTALL_NO_MOD_FILE;
		goto loser;
	}

	/*
	// Execute any executable files
	*/
	{
		argv[1] = NULL;
		envp[0] = NULL;
		for(execNode = executables.head; execNode; execNode = execNode->next) {
			attr = PR_NewProcessAttr();
			argv[0] = PR_Strdup(execNode->str);

			/* Announce our intentions */
			if(feedback) {
				PR_fprintf(feedback, msgStrings[EXEC_FILE_MSG], execNode->str);
			}

			/* start the process */
			if( !(proc=PR_CreateProcess(execNode->str, argv, envp, attr)) ) {
				PR_Free(argv[0]);
				PR_DestroyProcessAttr(attr);
				error(PK11_INSTALL_EXEC_FILE, execNode->str);
				ret=PK11_INSTALL_EXEC_FILE;
				goto loser;
			}

			/* wait for it to finish */
			if( PR_WaitProcess(proc, &errcode) != PR_SUCCESS) {
				PR_Free(argv[0]);
				PR_DestroyProcessAttr(attr);
				error(PK11_INSTALL_WAIT_PROCESS, execNode->str);
				ret=PK11_INSTALL_WAIT_PROCESS;
				goto loser;
			}

			/* What happened? */
			if(errcode) {
				/* process returned an error */
				error(PK11_INSTALL_PROC_ERROR, execNode->str, errcode);
			} else if(feedback) {
				/* process ran successfully */
				PR_fprintf(feedback, msgStrings[EXEC_SUCCESS], execNode->str);
			}

			PR_Free(argv[0]);
			PR_DestroyProcessAttr(attr);
		}
	}

	/*
	// Add the module
	*/
	status = Pk11Install_AddNewModule((char*)platform->moduleName,
		(char*)modDest, platform->mechFlags, platform->cipherFlags );

	if(status != SECSuccess) {
		error(PK11_INSTALL_ADD_MODULE, platform->moduleName);
		ret=PK11_INSTALL_ADD_MODULE;
		goto loser;
	}
	if(feedback) {
		PR_fprintf(feedback, msgStrings[INSTALLED_MODULE_MSG],
			platform->moduleName);
	}

	if(feedback) {
		PR_fprintf(feedback, msgStrings[INSTALLATION_COMPLETE_MSG]);
	}

	ret = PK11_INSTALL_SUCCESS;

loser:
	if(reldir) {
		PR_Free(reldir);
	}
	if(dest) {
		PR_Free(dest);
	}
	if(modDest) {
		PR_Free(modDest);
	}
	if(tempname) {
		PRFileInfo info;
		if(PR_GetFileInfo(tempname, &info) == PR_SUCCESS) {
			if(info.type == PR_FILE_DIRECTORY) {
				/* Recursively remove temporary directory */
				if(rm_dash_r(tempname)) {
					error(PK11_INSTALL_REMOVE_DIR,
						tempname);
					ret=PK11_INSTALL_REMOVE_DIR;
				}
					
			}
		}
		PR_Free(tempname);
	}
	StringList_delete(&executables);
	return ret;
}

/*
//////////////////////////////////////////////////////////////////////////
*/
static char*
PR_Strdup(const char* str)
{
	char *tmp = (char*) PR_Malloc(strlen(str)+1);
	strcpy(tmp, str);
	return tmp;
}

/*
 *  r m _ d a s h _ r
 *
 *  Remove a file, or a directory recursively.
 *
 */
static int
rm_dash_r (char *path)
{
    PRDir   *dir;
    PRDirEntry *entry;
    PRFileInfo fileinfo;
    char filename[240];

    if(PR_GetFileInfo(path, &fileinfo) != PR_SUCCESS) {
        /*fprintf(stderr, "Error: Unable to access %s\n", filename);*/
        return -1;
    }
    if(fileinfo.type == PR_FILE_DIRECTORY) {

        dir = PR_OpenDir(path);
        if(!dir) {
            return -1;
        }

        /* Recursively delete all entries in the directory */
        while((entry = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL) {
            sprintf(filename, "%s/%s", path, entry->name);
            if(rm_dash_r(filename)) return -1;
        }

        if(PR_CloseDir(dir) != PR_SUCCESS) {
            return -1;
        }

        /* Delete the directory itself */
        if(PR_RmDir(path) != PR_SUCCESS) {
            return -1;
        }
    } else {
        if(PR_Delete(path) != PR_SUCCESS) {
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *
 * m a k e _ d i r s
 *
 * Ensure that the directory portion of the path exists.  This may require
 * making the directory, and its parent, and its parent's parent, etc.
 */
static int
make_dirs(char *path, int file_perms)
{
	char *Path;
	char *start;
	char *sep;
	int ret = 0;
	PRFileInfo info;

	if(!path) {
		return 0;
	}

	Path = PR_Strdup(path);
	start = strpbrk(Path, "/\\");
	if(!start) {
		return 0;
	}
	start++; /* start right after first slash */

	/* Each time through the loop add one more directory. */
	while( (sep=strpbrk(start, "/\\")) ) {
		*sep = '\0';

		if( PR_GetFileInfo(Path, &info) != PR_SUCCESS) {
			/* No such dir, we have to create it */
			if( PR_MkDir(Path, dir_perms(file_perms)) != PR_SUCCESS) {
				error(PK11_INSTALL_CREATE_DIR, Path);
				ret = PK11_INSTALL_CREATE_DIR;
				goto loser;
			}
		} else {
			/* something exists by this name, make sure it's a directory */
			if( info.type != PR_FILE_DIRECTORY ) {
				error(PK11_INSTALL_CREATE_DIR, Path);
				ret = PK11_INSTALL_CREATE_DIR;
				goto loser;
			}
		}

		/* If this is the lowest directory level, make sure it is writeable */
		if(!strpbrk(sep+1, "/\\")) {
			if( PR_Access(Path, PR_ACCESS_WRITE_OK)!=PR_SUCCESS) {
				error(PK11_INSTALL_DIR_NOT_WRITEABLE, Path);
				ret = PK11_INSTALL_DIR_NOT_WRITEABLE;
				goto loser;
			}
		}

		start = sep+1; /* start after the next slash */
		*sep = '/';
	}

loser:
	PR_Free(Path);
	return ret;
}

/*************************************************************************
 * d i r _ p e r m s
 * 
 * Guesses the desired permissions on a directory based on the permissions
 * of a file that will be stored in it. Give read, write, and
 * execute to the owner (so we can create the file), read and 
 * execute to anyone who has read permissions on the file, and write
 * to anyone who has write permissions on the file.
 */
static int
dir_perms(int perms)
{
	int ret = 0;

	/* owner */
	ret |= 0700;

	/* group */
	if(perms & 0040) {
		/* read on the file -> read and execute on the directory */
		ret |= 0050;
	}
	if(perms & 0020) {
		/* write on the file -> write on the directory */
		ret |= 0020;
	}

	/* others */
	if(perms & 0004) {
		/* read on the file -> read and execute on the directory */
		ret |= 0005;
	}
	if(perms & 0002) {
		/* write on the file -> write on the directory */
		ret |= 0002;
	}

	return ret;
}
