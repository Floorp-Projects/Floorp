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
 * Portions created by the Initial Developer are Copyright (C) 1994-2007
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
 *  The following code handles the storage of PKCS 11 modules used by the
 * NSS. For the rest of NSS, only one kind of database handle exists:
 *
 *     SFTKDBHandle
 *
 * There is one SFTKDBHandle for the each key database and one for each cert 
 * database. These databases are opened as associated pairs, one pair per
 * slot. SFTKDBHandles are reference counted objects.
 *
 * Each SFTKDBHandle points to a low level database handle (SDB). This handle
 * represents the underlying physical database. These objects are not 
 * reference counted, an are 'owned' by their respective SFTKDBHandles.
 *
 *  
 */
#include "sftkdb.h"
#include "sftkpars.h"
#include "prprf.h" 
#include "prsystem.h"
#include "lgglue.h"
#include "secmodt.h"
#if defined (_WIN32)
#include <io.h>
#endif

/****************************************************************
 *
 * Secmod database.
 *
 * The new secmod database is simply a text file with each of the module
 * entries. in the following form:
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

static char *
sftkdb_quote(const char *string, char quote)
{
    char *newString = 0;
    int escapes = 0, size = 0;
    const char *src;
    char *dest;

    size=2;
    for (src=string; *src ; src++) {
	if ((*src == quote) || (*src == '\\')) escapes++;
	size++;
    }

    dest = newString = PORT_ZAlloc(escapes+size+1); 
    if (newString == NULL) {
	return NULL;
    }

    *dest++=quote;
    for (src=string; *src; src++,dest++) {
	if ((*src == '\\') || (*src == quote)) {
	    *dest++ = '\\';
	}
	*dest = *src;
    }
    *dest=quote;

    return newString;
}

/*
 * Smart string cat functions. Automatically manage the memory.
 * The first parameter is the source string. If it's null, we 
 * allocate memory for it. If it's not, we reallocate memory
 * so the the concanenated string fits.
 */
static char *
sftkdb_DupnCat(char *baseString, const char *str, int str_len)
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

/* Same as sftkdb_DupnCat except it concatenates the full string, not a
 * partial one */
static char *
sftkdb_DupCat(char *baseString, const char *str)
{
    return sftkdb_DupnCat(baseString, str, PORT_Strlen(str));
}

/* function to free up all the memory associated with a null terminated
 * array of module specs */
static SECStatus
sftkdb_releaseSpecList(char **moduleSpecList)
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
sftkdb_growList(char ***pModuleList, int *useCount, int last)
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
char *sftk_getOldSecmodName(const char *dbname,const char *filename)
{
    char *file = NULL;
    char *dirPath = PORT_Strdup(dbname);
    char *sep;

    sep = PORT_Strrchr(dirPath,*PATH_SEPARATOR);
#ifdef WINDOWS
    if (!sep) {
	sep = PORT_Strrchr(dirPath,'/');
    }
#endif
    if (sep) {
	*(sep)=0;
    }
    file= PR_smprintf("%s"PATH_SEPARATOR"%s", dirPath, filename);
    PORT_Free(dirPath);
    return file;
}

#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <fcntl.h>

#ifndef WINCE
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
#endif

#define MAX_LINE_LENGTH 2048
#define SFTK_DEFAULT_INTERNAL_INIT1 "library= name=\"NSS Internal PKCS #11 Module\" parameters="
#define SFTK_DEFAULT_INTERNAL_INIT2 " NSS=\"Flags=internal,critical trustOrder=75 cipherOrder=100 slotParams=(1={"
#define SFTK_DEFAULT_INTERNAL_INIT3 " askpw=any timeout=30})\""

/*
 * Read all the existing modules in out of the file.
 */
char **
sftkdb_ReadSecmodDB(SDBType dbType, const char *appName, 
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

    if ((dbType == SDB_LEGACY) || (dbType == SDB_MULTIACCESS)) {
	return sftkdbCall_ReadSecmodDB(appName, filename, dbname, params, rw);
    }

    moduleList = (char **) PORT_ZAlloc(useCount*sizeof(char **));
    if (moduleList == NULL) return NULL;

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
		    moduleString = sftkdb_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
	        moduleString = sftkdb_DupCat(moduleString, line);
		if (moduleString == NULL) goto loser;
	    /* value is already quoted, just write it out */
	    } else if (value[1] == '"') {
		if (moduleString) {
		    moduleString = sftkdb_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
	        moduleString = sftkdb_DupCat(moduleString, line);
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
		paramsValue = sftkdb_quote(&value[1], '"');
		if (paramsValue == NULL) goto loser;
		continue;
	    } else {
	    /* may need to quote */
	        char *newLine;
		if (moduleString) {
		    moduleString = sftkdb_DupnCat(moduleString," ", 1);
		    if (moduleString == NULL) goto loser;
		}
		moduleString = sftkdb_DupnCat(moduleString,line,value-line+1);
		if (moduleString == NULL)  goto loser;
	        newLine = sftkdb_quote(&value[1],'"');
		if (newLine == NULL) goto loser;
		moduleString = sftkdb_DupCat(moduleString,newLine);
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
		    paramsValue = sftkdb_quote(params, '"');
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
		moduleString = sftkdb_DupnCat(moduleString," parameters=", 12);
		if (moduleString == NULL) goto loser;
		moduleString = sftkdb_DupCat(moduleString, paramsValue);
		if (moduleString == NULL) goto loser;
	    }
	    PORT_Free(paramsValue);
	    paramsValue = NULL;
	}

	if ((moduleCount+1) >= useCount) {
	    SECStatus rv;
	    rv = sftkdb_growList(&moduleList, &useCount,  moduleCount+1);
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
	char *olddbname = sftk_getOldSecmodName(dbname,filename);
	PRStatus status;
	char **oldModuleList;
	int i;

	/* couldn't get the old name */
	if (!olddbname) {
	    goto bail;
	}

	/* old one doesn't exist */
	status = PR_Access(olddbname, PR_ACCESS_EXISTS);
	if (status != PR_SUCCESS) {
	    goto bail;
	}

	oldModuleList = sftkdbCall_ReadSecmodDB(appName, filename, 
					olddbname, params, rw);
	/* old one had no modules */
	if (!oldModuleList) {
	    goto bail;
	}

	/* count the modules */
	for (i=0; oldModuleList[i]; i++) { }

	/* grow the moduleList if necessary */
	if (i >= useCount) {
	    SECStatus rv;
	    rv = sftkdb_growList(&moduleList,&useCount,moduleCount+1);
	    if (rv != SECSuccess) {
		goto loser;
	    }
	}
	
	/* write each module out, and copy it */
	for (i=0; oldModuleList[i]; i++) {
	    if (rw) {
		sftkdb_AddSecmodDB(dbType,appName,filename,dbname,
				oldModuleList[i],rw);
	    }
	    if (moduleList[i]) {
		PORT_Free(moduleList[i]);
	    }
	    moduleList[i] = PORT_Strdup(oldModuleList[i]);
	}

	/* done with the old module list */
	sftkdbCall_ReleaseSecmodDBData(appName, filename, olddbname, 
				  oldModuleList, rw);
bail:
	if (olddbname) {
	    PR_smprintf_free(olddbname);
	}
    }
	
    if (!moduleList[0]) {
	char * newParams;
	moduleString = PORT_Strdup(SFTK_DEFAULT_INTERNAL_INIT1);
	newParams = sftkdb_quote(params,'"');
	if (newParams == NULL) goto loser;
	moduleString = sftkdb_DupCat(moduleString, newParams);
	PORT_Free(newParams);
	if (moduleString == NULL) goto loser;
	moduleString = sftkdb_DupCat(moduleString, SFTK_DEFAULT_INTERNAL_INIT2);
	if (moduleString == NULL) goto loser;
	moduleString = sftkdb_DupCat(moduleString, SECMOD_SLOT_FLAGS);
	if (moduleString == NULL) goto loser;
	moduleString = sftkdb_DupCat(moduleString, SFTK_DEFAULT_INTERNAL_INIT3);
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
	sftkdb_releaseSpecList(moduleList);
	moduleList = NULL;
	failed = PR_TRUE;
    }
    if (fd != NULL) {
	fclose(fd);
    } else if (!failed && rw) {
	/* update our internal module */
	sftkdb_AddSecmodDB(dbType,appName,filename,dbname,moduleList[0],rw);
    }
    return moduleList;
}

SECStatus
sftkdb_ReleaseSecmodDBData(SDBType dbType, const char *appName, 
			const char *filename, const char *dbname, 
			char **moduleSpecList, PRBool rw)
{
    if ((dbType == SDB_LEGACY) || (dbType == SDB_MULTIACCESS)) {
	return sftkdbCall_ReleaseSecmodDBData(appName, filename, dbname, 
					  moduleSpecList, rw);
    }
    if (moduleSpecList) {
	sftkdb_releaseSpecList(moduleSpecList);
    }
    return SECSuccess;
}


/*
 * Delete a module from the Data Base
 */
SECStatus
sftkdb_DeleteSecmodDB(SDBType dbType, const char *appName, 
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

    if ((dbType == SDB_LEGACY) || (dbType == SDB_MULTIACCESS)) {
	return sftkdbCall_DeleteSecmodDB(appName, filename, dbname, args, rw);
    }

    if (!rw) {
	return SECFailure;
    }

    dbname2 = strdup(dbname);
    if (dbname2 == NULL) goto loser;
    dbname2[strlen(dbname)-1]++;

    /* do we really want to use streams here */
    fd = fopen(dbname, "r");
    if (fd == NULL) goto loser;
#ifdef WINCE
    fd2 = fopen(dbname2, "w+");
#else
    fd2 = lfopen(dbname2, "w+", O_CREAT|O_RDWR|O_TRUNC);
#endif
    if (fd2 == NULL) goto loser;

    name = sftk_argGetParamValue("name",args);
    if (name) {
	name_len = PORT_Strlen(name);
    }
    lib = sftk_argGetParamValue("library",args);
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
	    block = sftkdb_DupCat(block,line);
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
SECStatus
sftkdb_AddSecmodDB(SDBType dbType, const char *appName, 
		   const char *filename, const char *dbname, 
		   char *module, PRBool rw)
{
    FILE *fd = NULL;
    char *block = NULL;
    PRBool libFound = PR_FALSE;

    if ((dbType == SDB_LEGACY) || (dbType == SDB_MULTIACCESS)) {
	return sftkdbCall_AddSecmodDB(appName, filename, dbname, module, rw);
    }

    /* can't write to a read only module */
    if (!rw) {
	return SECFailure;
    }

    /* remove the previous version if it exists */
    (void) sftkdb_DeleteSecmodDB(dbType, appName, filename, dbname, module, rw);

#ifdef WINCE
    fd = fopen(dbname, "a+");
#else
    fd = lfopen(dbname, "a+", O_CREAT|O_RDWR|O_APPEND);
#endif
    if (fd == NULL) {
	return SECFailure;
    }
    module = sftk_argStrip(module);
    while (*module) {
	int count;
	char *keyEnd = PORT_Strchr(module,'=');
	char *value;

	if (PORT_Strncmp(module, "library=", 8) == 0) {
	   libFound=PR_TRUE;
	}
	if (keyEnd == NULL) {
	    block = sftkdb_DupCat(block, module);
	    break;
	}
	block = sftkdb_DupnCat(block, module, keyEnd-module+1);
	if (block == NULL) { goto loser; }
	value = sftk_argFetchValue(&keyEnd[1], &count);
	if (value) {
	    block = sftkdb_DupCat(block, sftk_argStrip(value));
	    PORT_Free(value);
	}
	if (block == NULL) { goto loser; }
	block = sftkdb_DupnCat(block, "\n", 1);
	module = keyEnd + 1 + count;
	module = sftk_argStrip(module);
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
  

