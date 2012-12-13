/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* 
 * The following code handles the storage of PKCS 11 modules used by the
 * NSS. For the rest of NSS, only one kind of database handle exists:
 *
 *     SFTKDBHandle
 *
 * There is one SFTKDBHandle for each key database and one for each cert 
 * database. These databases are opened as associated pairs, one pair per
 * slot. SFTKDBHandles are reference counted objects.
 *
 * Each SFTKDBHandle points to a low level database handle (SDB). This handle
 * represents the underlying physical database. These objects are not 
 * reference counted, and are 'owned' by their respective SFTKDBHandles.
 */

#include "prprf.h" 
#include "prsystem.h"
#include "secport.h"
#include "utilpars.h" 
#include "secerr.h"
#if defined (_WIN32)
#include <io.h>
#endif

/****************************************************************
 *
 * Secmod database.
 *
 * The new secmod database is simply a text file with each of the module
 * entries in the following form:
 *
 * #
 * # This is a comment The next line is the library to load
 * library=libmypkcs11.so
 * name="My PKCS#11 module"
 * params="my library's param string"
 * nss="NSS parameters"
 * other="parameters for other libraries and applications"
 * 
 * library=libmynextpk11.so
 * name="My other PKCS#11 module"
 */


/*
 * Smart string cat functions. Automatically manage the memory.
 * The first parameter is the source string. If it's null, we 
 * allocate memory for it. If it's not, we reallocate memory
 * so the the concanenated string fits.
 */
static char *
nssutil_DupnCat(char *baseString, const char *str, int str_len)
{
    int len = (baseString ? PORT_Strlen(baseString) : 0) + 1;
    char *newString;

    len += str_len;
    newString = (char *) PORT_Realloc(baseString,len);
    if (newString == NULL) {
	PORT_Free(baseString);
	return NULL;
    }
    if (baseString == NULL) *newString = 0;
    return PORT_Strncat(newString,str, str_len);
}

/* Same as nssutil_DupnCat except it concatenates the full string, not a
 * partial one */
static char *
nssutil_DupCat(char *baseString, const char *str)
{
    return nssutil_DupnCat(baseString, str, PORT_Strlen(str));
}

/* function to free up all the memory associated with a null terminated
 * array of module specs */
static SECStatus
nssutil_releaseSpecList(char **moduleSpecList)
{
    if (moduleSpecList) {
	char **index;
	for(index = moduleSpecList; *index; index++) {
	    PORT_Free(*index);
	}
	PORT_Free(moduleSpecList);
    }
    return SECSuccess;
}

#define SECMOD_STEP 10
static SECStatus
nssutil_growList(char ***pModuleList, int *useCount, int last)
{
    char **newModuleList;

    *useCount += SECMOD_STEP;
    newModuleList = (char **)PORT_Realloc(*pModuleList,
					  *useCount*sizeof(char *));
    if (newModuleList == NULL) {
	return SECFailure;
    }
    PORT_Memset(&newModuleList[last],0, sizeof(char *)*SECMOD_STEP);
    *pModuleList = newModuleList;
    return SECSuccess;
}

static 
char *_NSSUTIL_GetOldSecmodName(const char *dbname,const char *filename)
{
    char *file = NULL;
    char *dirPath = PORT_Strdup(dbname);
    char *sep;

    sep = PORT_Strrchr(dirPath,*NSSUTIL_PATH_SEPARATOR);
#ifdef _WIN32
    if (!sep) {
	/* utilparst.h defines NSSUTIL_PATH_SEPARATOR as "/" for all
	 * platforms. */
	sep = PORT_Strrchr(dirPath,'\\');
    }
#endif
    if (sep) {
	*sep = 0;
	file = PR_smprintf("%s"NSSUTIL_PATH_SEPARATOR"%s", dirPath, filename);
    } else {
	file = PR_smprintf("%s", filename);
    }
    PORT_Free(dirPath);
    return file;
}

static SECStatus nssutil_AddSecmodDB(const char *appName, 
		   const char *filename, const char *dbname, 
		   char *module, PRBool rw);

#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <fcntl.h>

/* same as fopen, except it doesn't use umask, but explicit */
FILE *
lfopen(const char *name, const char *mode, int flags)
{
    int fd;
    FILE *file;

    fd = open(name, flags, 0600);
    if (fd < 0) {
	return NULL;
    }
    file = fdopen(fd, mode);
    if (!file) {
	close(fd);
    }
    /* file inherits fd */
    return file;
}

#define MAX_LINE_LENGTH 2048

/*
 * Read all the existing modules in out of the file.
 */
static char **
nssutil_ReadSecmodDB(const char *appName, 
		    const char *filename, const char *dbname, 
		    char *params, PRBool rw)
{
    FILE *fd = NULL;
    char **moduleList = NULL;
    int moduleCount = 1;
    int useCount = SECMOD_STEP;
    char line[MAX_LINE_LENGTH];
    PRBool internal = PR_FALSE;
    PRBool skipParams = PR_FALSE;
    char *moduleString = NULL;
    char *paramsValue=NULL;
    PRBool failed = PR_TRUE;

    moduleList = (char **) PORT_ZAlloc(useCount*sizeof(char **));
    if (moduleList == NULL) return NULL;

    if (dbname == NULL) {
	goto return_default;
    }

    /* do we really want to use streams here */
    fd = fopen(dbname, "r");
    if (fd == NULL) goto done;

    /*
     * the following loop takes line separated config lines and collapses
     * the lines to a single string, escaping and quoting as necessary.
     */
    /* loop state variables */
    moduleString = NULL;  /* current concatenated string */
    internal = PR_FALSE;	     /* is this an internal module */
    skipParams = PR_FALSE;	   /* did we find an override parameter block*/
    paramsValue = NULL;		   /* the current parameter block value */
    while (fgets(line, sizeof(line), fd) != NULL) { 
	int len = PORT_Strlen(line);

	/* remove the ending newline */
	if (len && line[len-1] == '\n') {
	    len--;
	    line[len] = 0;
	}
	if (*line == '#') {
	    continue;
	}
	if (*line != 0) {
	    /*
	     * The PKCS #11 group standard assumes blocks of strings
	     * separated by new lines, clumped by new lines. Internally
	     * we take strings separated by spaces, so we may need to escape
	     * certain spaces.
	     */
	    char *value = PORT_Strchr(line,'=');

	    /* there is no value, write out the stanza as is */
	    if (value == NULL || value[1] == 0) {
		if (moduleString) {
		    moduleString = nssutil_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
	        moduleString = nssutil_DupCat(moduleString, line);
		if (moduleString == NULL) goto loser;
	    /* value is already quoted, just write it out */
	    } else if (value[1] == '"') {
		if (moduleString) {
		    moduleString = nssutil_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
	        moduleString = nssutil_DupCat(moduleString, line);
		if (moduleString == NULL) goto loser;
		/* we have an override parameter section, remember that
		 * we found this (see following comment about why this
		 * is necessary). */
	        if (PORT_Strncasecmp(line, "parameters", 10) == 0) {
			skipParams = PR_TRUE;
		}
	    /*
	     * The internal token always overrides it's parameter block
	     * from the passed in parameters, so wait until then end
	     * before we include the parameter block in case we need to 
	     * override it. NOTE: if the parameter block is quoted with ("),
	     * this override does not happen. This allows you to override
	     * the application's parameter configuration.
	     *
	     * parameter block state is controlled by the following variables:
	     *  skipParams - Bool : set to true of we have an override param
	     *    block (all other blocks, either implicit or explicit are
	     *    ignored).
	     *  paramsValue - char * : pointer to the current param block. In
	     *    the absence of overrides, paramsValue is set to the first
	     *    parameter block we find. All subsequent blocks are ignored.
	     *    When we find an internal token, the application passed
	     *    parameters take precident.
	     */
	    } else if (PORT_Strncasecmp(line, "parameters", 10) == 0) {
		/* already have parameters */
		if (paramsValue) {
			continue;
		}
		paramsValue = NSSUTIL_Quote(&value[1], '"');
		if (paramsValue == NULL) goto loser;
		continue;
	    } else {
	    /* may need to quote */
	        char *newLine;
		if (moduleString) {
		    moduleString = nssutil_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
		moduleString = nssutil_DupnCat(moduleString,line,value-line+1);
		if (moduleString == NULL)  goto loser;
	        newLine = NSSUTIL_Quote(&value[1],'"');
		if (newLine == NULL) goto loser;
		moduleString = nssutil_DupCat(moduleString,newLine);
	        PORT_Free(newLine);
		if (moduleString == NULL) goto loser;
	    }

	    /* check to see if it's internal? */
	    if (PORT_Strncasecmp(line, "NSS=", 4) == 0) {
		/* This should be case insensitive! reviewers make
		 * me fix it if it's not */
		if (PORT_Strstr(line,"internal")) {
		    internal = PR_TRUE;
		    /* override the parameters */
		    if (paramsValue) {
			PORT_Free(paramsValue);
		    }
		    paramsValue = NSSUTIL_Quote(params, '"');
		}
	    }
	    continue;
	}
	if ((moduleString == NULL) || (*moduleString == 0)) {
	    continue;
	}

	/* 
	 * if we are here, we have found a complete stanza. Now write out
	 * any param section we may have found.
	 */
	if (paramsValue) {
	    /* we had an override */
	    if (!skipParams) {
		moduleString = nssutil_DupnCat(moduleString," parameters=", 12);
		if (moduleString == NULL) goto loser;
		moduleString = nssutil_DupCat(moduleString, paramsValue);
		if (moduleString == NULL) goto loser;
	    }
	    PORT_Free(paramsValue);
	    paramsValue = NULL;
	}

	if ((moduleCount+1) >= useCount) {
	    SECStatus rv;
	    rv = nssutil_growList(&moduleList, &useCount,  moduleCount+1);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}

	if (internal) {
	    moduleList[0] = moduleString;
	} else {
	    moduleList[moduleCount] = moduleString;
	    moduleCount++;
	}
	moduleString = NULL;
	internal = PR_FALSE;
	skipParams = PR_FALSE;
    } 

    if (moduleString) {
	PORT_Free(moduleString);
	moduleString = NULL;
    }
done:
    /* if we couldn't open a pkcs11 database, look for the old one */
    if (fd == NULL) {
	char *olddbname = _NSSUTIL_GetOldSecmodName(dbname,filename);
	PRStatus status;

	/* couldn't get the old name */
	if (!olddbname) {
	    goto bail;
	}

	/* old one exists */
	status = PR_Access(olddbname, PR_ACCESS_EXISTS);
	if (status == PR_SUCCESS) {
	    PR_smprintf_free(olddbname);
	    PORT_SetError(SEC_ERROR_LEGACY_DATABASE);
	    return NULL;
	}

bail:
	if (olddbname) {
	    PR_smprintf_free(olddbname);
	}
    }

return_default:
	
    if (!moduleList[0]) {
	char * newParams;
	moduleString = PORT_Strdup(NSSUTIL_DEFAULT_INTERNAL_INIT1);
	newParams = NSSUTIL_Quote(params,'"');
	if (newParams == NULL) goto loser;
	moduleString = nssutil_DupCat(moduleString, newParams);
	PORT_Free(newParams);
	if (moduleString == NULL) goto loser;
	moduleString = nssutil_DupCat(moduleString, 
					NSSUTIL_DEFAULT_INTERNAL_INIT2);
	if (moduleString == NULL) goto loser;
	moduleString = nssutil_DupCat(moduleString,
					NSSUTIL_DEFAULT_SFTKN_FLAGS);
	if (moduleString == NULL) goto loser;
	moduleString = nssutil_DupCat(moduleString, 
					NSSUTIL_DEFAULT_INTERNAL_INIT3);
	if (moduleString == NULL) goto loser;
	moduleList[0] = moduleString;
	moduleString = NULL;
    }
    failed = PR_FALSE;

loser:
    /*
     * cleanup
     */
    /* deal with trust cert db here */
    if (moduleString) {
	PORT_Free(moduleString);
	moduleString = NULL;
    }
    if (paramsValue) {
	PORT_Free(paramsValue);
	paramsValue = NULL;
    }
    if (failed || (moduleList[0] == NULL)) {
	/* This is wrong! FIXME */
	nssutil_releaseSpecList(moduleList);
	moduleList = NULL;
	failed = PR_TRUE;
    }
    if (fd != NULL) {
	fclose(fd);
    } else if (!failed && rw) {
	/* update our internal module */
	nssutil_AddSecmodDB(appName,filename,dbname,moduleList[0],rw);
    }
    return moduleList;
}

static SECStatus
nssutil_ReleaseSecmodDBData(const char *appName, 
			const char *filename, const char *dbname, 
			char **moduleSpecList, PRBool rw)
{
    if (moduleSpecList) {
	nssutil_releaseSpecList(moduleSpecList);
    }
    return SECSuccess;
}


/*
 * Delete a module from the Data Base
 */
static SECStatus
nssutil_DeleteSecmodDB(const char *appName, 
		      const char *filename, const char *dbname, 
		      char *args, PRBool rw)
{
    /* SHDB_FIXME implement */
    FILE *fd = NULL;
    FILE *fd2 = NULL;
    char line[MAX_LINE_LENGTH];
    char *dbname2 = NULL;
    char *block = NULL;
    char *name = NULL;
    char *lib = NULL;
    int name_len, lib_len;
    PRBool skip = PR_FALSE;
    PRBool found = PR_FALSE;

    if (dbname == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    if (!rw) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	return SECFailure;
    }

    dbname2 = PORT_Strdup(dbname);
    if (dbname2 == NULL) goto loser;
    dbname2[strlen(dbname)-1]++;

    /* do we really want to use streams here */
    fd = fopen(dbname, "r");
    if (fd == NULL) goto loser;
    fd2 = lfopen(dbname2, "w+", O_CREAT|O_RDWR|O_TRUNC);
    if (fd2 == NULL) goto loser;

    name = NSSUTIL_ArgGetParamValue("name",args);
    if (name) {
	name_len = PORT_Strlen(name);
    }
    lib = NSSUTIL_ArgGetParamValue("library",args);
    if (lib) {
	lib_len = PORT_Strlen(lib);
    }


    /*
     * the following loop takes line separated config files and collapses
     * the lines to a single string, escaping and quoting as necessary.
     */
    /* loop state variables */
    block = NULL;
    skip = PR_FALSE;
    while (fgets(line, sizeof(line), fd) != NULL) { 
	/* If we are processing a block (we haven't hit a blank line yet */
	if (*line != '\n') {
	    /* skip means we are in the middle of a block we are deleting */
	    if (skip) {
		continue;
	    }
	    /* if we haven't found the block yet, check to see if this block
	     * matches our requirements */
	    if (!found && ((name && (PORT_Strncasecmp(line,"name=",5) == 0) &&
		 (PORT_Strncmp(line+5,name,name_len) == 0))  ||
	        (lib && (PORT_Strncasecmp(line,"library=",8) == 0) &&
		 (PORT_Strncmp(line+8,lib,lib_len) == 0)))) {

		/* yup, we don't need to save any more data, */
		PORT_Free(block);
		block=NULL;
		/* we don't need to collect more of this block */
		skip = PR_TRUE;
		/* we don't need to continue searching for the block */
		found =PR_TRUE;
		continue;
	    }
	    /* not our match, continue to collect data in this block */
	    block = nssutil_DupCat(block,line);
	    continue;
	}
	/* we've collected a block of data that wasn't the module we were
	 * looking for, write it out */
	if (block) {
	    fwrite(block, PORT_Strlen(block), 1, fd2);
	    PORT_Free(block);
	    block = NULL;
	}
	/* If we didn't just delete the this block, keep the blank line */
	if (!skip) {
	    fputs(line,fd2);
	}
	/* we are definately not in a deleted block anymore */
	skip = PR_FALSE;
    } 
    fclose(fd);
    fclose(fd2);
    if (found) {
	/* rename dbname2 to dbname */
	PR_Delete(dbname);
	PR_Rename(dbname2,dbname);
    } else {
	PR_Delete(dbname2);
    }
    PORT_Free(dbname2);
    PORT_Free(lib);
    PORT_Free(name);
    PORT_Free(block);
    return SECSuccess;

loser:
    if (fd != NULL) {
	fclose(fd);
    }
    if (fd2 != NULL) {
	fclose(fd2);
    }
    if (dbname2) {
	PR_Delete(dbname2);
	PORT_Free(dbname2);
    }
    PORT_Free(lib);
    PORT_Free(name);
    return SECFailure;
}

/*
 * Add a module to the Data base 
 */
static SECStatus
nssutil_AddSecmodDB(const char *appName, 
		   const char *filename, const char *dbname, 
		   char *module, PRBool rw)
{
    FILE *fd = NULL;
    char *block = NULL;
    PRBool libFound = PR_FALSE;

    if (dbname == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* can't write to a read only module */
    if (!rw) {
	PORT_SetError(SEC_ERROR_READ_ONLY);
	return SECFailure;
    }

    /* remove the previous version if it exists */
    (void) nssutil_DeleteSecmodDB(appName, filename, 
				  dbname, module, rw);

    fd = lfopen(dbname, "a+", O_CREAT|O_RDWR|O_APPEND);
    if (fd == NULL) {
	return SECFailure;
    }
    module = NSSUTIL_ArgStrip(module);
    while (*module) {
	int count;
	char *keyEnd = PORT_Strchr(module,'=');
	char *value;

	if (PORT_Strncmp(module, "library=", 8) == 0) {
	   libFound=PR_TRUE;
	}
	if (keyEnd == NULL) {
	    block = nssutil_DupCat(block, module);
	    break;
	}
	block = nssutil_DupnCat(block, module, keyEnd-module+1);
	if (block == NULL) { goto loser; }
	value = NSSUTIL_ArgFetchValue(&keyEnd[1], &count);
	if (value) {
	    block = nssutil_DupCat(block, NSSUTIL_ArgStrip(value));
	    PORT_Free(value);
	}
	if (block == NULL) { goto loser; }
	block = nssutil_DupnCat(block, "\n", 1);
	module = keyEnd + 1 + count;
	module = NSSUTIL_ArgStrip(module);
    }
    if (block) {
	if (!libFound) {
	    fprintf(fd,"library=\n");
	}
	fwrite(block, PORT_Strlen(block), 1, fd);
	fprintf(fd,"\n");
	PORT_Free(block);
	block = NULL;
    }
    fclose(fd);
    return SECSuccess;

loser:
    PORT_Free(block);
    fclose(fd);
    return SECFailure;
}
  

char **
NSSUTIL_DoModuleDBFunction(unsigned long function,char *parameters, void *args)
{
    char *secmod = NULL;
    char *appName = NULL;
    char *filename = NULL;
    NSSDBType dbType = NSS_DB_TYPE_NONE;
    PRBool rw;
    static char *success="Success";
    char **rvstr = NULL;


    secmod = _NSSUTIL_GetSecmodName(parameters, &dbType, &appName,
				    &filename, &rw);
    if ((dbType == NSS_DB_TYPE_LEGACY) || 
	 (dbType == NSS_DB_TYPE_MULTIACCESS)) {
	/* we can't handle the old database, only softoken can */
	PORT_SetError(SEC_ERROR_LEGACY_DATABASE);
	rvstr =  NULL;
	goto done;
    }

    switch (function) {
    case SECMOD_MODULE_DB_FUNCTION_FIND:
        rvstr = nssutil_ReadSecmodDB(appName,filename,
				     secmod,(char *)parameters,rw);
        break;
    case SECMOD_MODULE_DB_FUNCTION_ADD:
        rvstr = (nssutil_AddSecmodDB(appName,filename,
		secmod,(char *)args,rw) == SECSuccess) ? &success: NULL;
        break;
    case SECMOD_MODULE_DB_FUNCTION_DEL:
        rvstr = (nssutil_DeleteSecmodDB(appName,filename,
		secmod,(char *)args,rw) == SECSuccess) ? &success: NULL;
        break;
    case SECMOD_MODULE_DB_FUNCTION_RELEASE:
        rvstr = (nssutil_ReleaseSecmodDBData(appName,filename,
		secmod, (char **)args,rw) == SECSuccess) ? &success: NULL;
        break;
    }
done:
    if (secmod) PR_smprintf_free(secmod);
    if (appName) PORT_Free(appName);
    if (filename) PORT_Free(filename);
    return rvstr;
}
