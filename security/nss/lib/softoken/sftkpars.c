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
 * NSS. This file is written to abstract away how the modules are
 * stored so we can deside that later.
 */
#include "sftkpars.h"
#include "pkcs11i.h"
#include "sdb.h"
#include "prprf.h" 
#include "prenv.h"

/*
 * this file contains routines for parsing PKCS #11 module spec
 * strings.
 */

#define SFTK_HANDLE_STRING_ARG(param,target,value,command) \
    if (PORT_Strncasecmp(param,value,sizeof(value)-1) == 0) { \
	param += sizeof(value)-1; \
	target = sftk_argFetchValue(param,&next); \
	param += next; \
	command ;\
    } else  

#define SFTK_HANDLE_FINAL_ARG(param) \
    { param = sftk_argSkipParameter(param); } param = sftk_argStrip(param);

static PRBool sftk_argGetPair(char c) {
    switch (c) {
    case '\'': return c;
    case '\"': return c;
    case '<': return '>';
    case '{': return '}';
    case '[': return ']';
    case '(': return ')';
    default: break;
    }
    return ' ';
}

static PRBool sftk_argIsBlank(char c) {
   return isspace((unsigned char )c);
}

static PRBool sftk_argIsEscape(char c) {
    return c == '\\';
}

static PRBool sftk_argIsQuote(char c) {
    switch (c) {
    case '\'':
    case '\"':
    case '<':
    case '{': /* } end curly to keep vi bracket matching working */
    case '(': /* ) */
    case '[': /* ] */ return PR_TRUE;
    default: break;
    }
    return PR_FALSE;
}

char *sftk_argStrip(char *c) {
   while (*c && sftk_argIsBlank(*c)) c++;
   return c;
}

static char *
sftk_argFindEnd(char *string) {
    char endChar = ' ';
    PRBool lastEscape = PR_FALSE;

    if (sftk_argIsQuote(*string)) {
	endChar = sftk_argGetPair(*string);
	string++;
    }

    for (;*string; string++) {
	if (lastEscape) {
	    lastEscape = PR_FALSE;
	    continue;
	}
	if (sftk_argIsEscape(*string) && !lastEscape) {
	    lastEscape = PR_TRUE;
	    continue;
	} 
	if ((endChar == ' ') && sftk_argIsBlank(*string)) break;
	if (*string == endChar) {
	    break;
	}
    }

    return string;
}

char *
sftk_argFetchValue(char *string, int *pcount)
{
    char *end = sftk_argFindEnd(string);
    char *retString, *copyString;
    PRBool lastEscape = PR_FALSE;
    int len;

    len = end - string;
    if (len == 0) {
	*pcount = 0;
	return NULL;
    }

    copyString = retString = (char *)PORT_Alloc(len+1);

    if (*end) len++;
    *pcount = len;
    if (retString == NULL) return NULL;


    if (sftk_argIsQuote(*string)) string++;
    for (; string < end; string++) {
	if (sftk_argIsEscape(*string) && !lastEscape) {
	    lastEscape = PR_TRUE;
	    continue;
	}
	lastEscape = PR_FALSE;
	*copyString++ = *string;
    }
    *copyString = 0;
    return retString;
}

static char *
sftk_argSkipParameter(char *string) 
{
     char *end;
     /* look for the end of the <name>= */
     for (;*string; string++) {
	if (*string == '=') { string++; break; }
	if (sftk_argIsBlank(*string)) return(string); 
     }

     end = sftk_argFindEnd(string);
     if (*end) end++;
     return end;
}

char *
sftk_argGetParamValue(char *paramName,char *parameters)
{
    char searchValue[256];
    int paramLen = strlen(paramName);
    char *returnValue = NULL;
    int next;

    if ((parameters == NULL) || (*parameters == 0)) return NULL;

    PORT_Assert(paramLen+2 < sizeof(searchValue));

    PORT_Strcpy(searchValue,paramName);
    PORT_Strcat(searchValue,"=");
    while (*parameters) {
	if (PORT_Strncasecmp(parameters,searchValue,paramLen+1) == 0) {
	    parameters += paramLen+1;
	    returnValue = sftk_argFetchValue(parameters,&next);
	    break;
	} else {
	    parameters = sftk_argSkipParameter(parameters);
	}
	parameters = sftk_argStrip(parameters);
   }
   return returnValue;
}
    
static char *
sftk_argNextFlag(char *flags)
{
    for (; *flags ; flags++) {
	if (*flags == ',') {
	    flags++;
	    break;
	}
    }
    return flags;
}

static PRBool
sftk_argHasFlag(char *label, char *flag, char *parameters)
{
    char *flags,*index;
    int len = strlen(flag);
    PRBool found = PR_FALSE;

    flags = sftk_argGetParamValue(label,parameters);
    if (flags == NULL) return PR_FALSE;

    for (index=flags; *index; index=sftk_argNextFlag(index)) {
	if (PORT_Strncasecmp(index,flag,len) == 0) {
	    found=PR_TRUE;
	    break;
	}
    }
    PORT_Free(flags);
    return found;
}

/*
 * decode a number. handle octal (leading '0'), hex (leading '0x') or decimal
 */
static long
sftk_argDecodeNumber(char *num)
{
    int	radix = 10;
    unsigned long value = 0;
    long retValue = 0;
    int sign = 1;
    int digit;

    if (num == NULL) return retValue;

    num = sftk_argStrip(num);

    if (*num == '-') {
	sign = -1;
	num++;
    }

    if (*num == '0') {
	radix = 8;
	num++;
	if ((*num == 'x') || (*num == 'X')) {
	    radix = 16;
	    num++;
	}
    }

   
    for ( ;*num; num++ ) {
	if (isdigit(*num)) {
	    digit = *num - '0';
	} else if ((*num >= 'a') && (*num <= 'f'))  {
	    digit = *num - 'a' + 10;
	} else if ((*num >= 'A') && (*num <= 'F'))  {
	    digit = *num - 'A' + 10;
	} else {
	    break;
	}
	if (digit >= radix) break;
	value = value*radix + digit;
    }

    retValue = ((int) value) * sign;
    return retValue;
}

static char *
sftk_argGetName(char *inString, int *next) 
{
    char *name=NULL;
    char *string;
    int len;

    /* look for the end of the <name>= */
    for (string = inString;*string; string++) {
	if (*string == '=') { break; }
	if (sftk_argIsBlank(*string)) break;
    }

    len = string - inString;

    *next = len; 
    if (*string == '=') (*next) += 1;
    if (len > 0) {
	name = PORT_Alloc(len+1);
	PORT_Strncpy(name,inString,len);
	name[len] = 0;
    }
    return name;
}

#define FREE_CLEAR(p) if (p) { PORT_Free(p); p = NULL; }

static void
sftk_parseTokenFlags(char *tmp, sftk_token_parameters *parsed) { 
    parsed->readOnly = sftk_argHasFlag("flags","readOnly",tmp);
    parsed->noCertDB = sftk_argHasFlag("flags","noCertDB",tmp);
    parsed->noKeyDB = sftk_argHasFlag("flags","noKeyDB",tmp);
    parsed->forceOpen = sftk_argHasFlag("flags","forceOpen",tmp);
    parsed->pwRequired = sftk_argHasFlag("flags","passwordRequired",tmp);
    parsed->optimizeSpace = sftk_argHasFlag("flags","optimizeSpace",tmp);
    return;
}

static void
sftk_parseFlags(char *tmp, sftk_parameters *parsed) { 
    parsed->noModDB = sftk_argHasFlag("flags","noModDB",tmp);
    parsed->readOnly = sftk_argHasFlag("flags","readOnly",tmp);
    /* keep legacy interface working */
    parsed->noCertDB = sftk_argHasFlag("flags","noCertDB",tmp);
    parsed->forceOpen = sftk_argHasFlag("flags","forceOpen",tmp);
    parsed->pwRequired = sftk_argHasFlag("flags","passwordRequired",tmp);
    parsed->optimizeSpace = sftk_argHasFlag("flags","optimizeSpace",tmp);
    return;
}

static CK_RV
sftk_parseTokenParameters(char *param, sftk_token_parameters *parsed) 
{
    int next;
    char *tmp;
    char *index;
    index = sftk_argStrip(param);

    while (*index) {
	SFTK_HANDLE_STRING_ARG(index,parsed->configdir,"configDir=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updatedir,"updateDir=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updCertPrefix,"updateCertPrefix=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updKeyPrefix,"updateKeyPrefix=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updateID,"updateID=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->certPrefix,"certPrefix=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->keyPrefix,"keyPrefix=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->tokdes,"tokenDescription=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updtokdes,
						"updateTokenDescription=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->slotdes,"slotDescription=",;)
	SFTK_HANDLE_STRING_ARG(index,tmp,"minPWLen=", 
			if(tmp) { parsed->minPW=atoi(tmp); PORT_Free(tmp); })
	SFTK_HANDLE_STRING_ARG(index,tmp,"flags=", 
	   if(tmp) { sftk_parseTokenFlags(param,parsed); PORT_Free(tmp); })
	SFTK_HANDLE_FINAL_ARG(index)
   }
   return CKR_OK;
}

static void
sftk_parseTokens(char *tokenParams, sftk_parameters *parsed)
{
    char *tokenIndex;
    sftk_token_parameters *tokens = NULL;
    int i=0,count = 0,next;

    if ((tokenParams == NULL) || (*tokenParams == 0))  return;

    /* first count the number of slots */
    for (tokenIndex = sftk_argStrip(tokenParams); *tokenIndex;
	 tokenIndex = sftk_argStrip(sftk_argSkipParameter(tokenIndex))) {
	count++;
    }

    /* get the data structures */
    tokens = (sftk_token_parameters *) 
			PORT_ZAlloc(count*sizeof(sftk_token_parameters));
    if (tokens == NULL) return;

    for (tokenIndex = sftk_argStrip(tokenParams), i = 0;
					*tokenIndex && i < count ; i++ ) {
	char *name;
	name = sftk_argGetName(tokenIndex,&next);
	tokenIndex += next;

	tokens[i].slotID = sftk_argDecodeNumber(name);
        tokens[i].readOnly = PR_FALSE;
	tokens[i].noCertDB = PR_FALSE;
	tokens[i].noKeyDB = PR_FALSE;
	if (!sftk_argIsBlank(*tokenIndex)) {
	    char *args = sftk_argFetchValue(tokenIndex,&next);
	    tokenIndex += next;
	    if (args) {
		sftk_parseTokenParameters(args,&tokens[i]);
		PORT_Free(args);
	    }
	}
	if (name) PORT_Free(name);
	tokenIndex = sftk_argStrip(tokenIndex);
    }
    parsed->token_count = i;
    parsed->tokens = tokens;
    return; 
}

CK_RV
sftk_parseParameters(char *param, sftk_parameters *parsed, PRBool isFIPS) 
{
    int next;
    char *tmp;
    char *index;
    char *certPrefix = NULL, *keyPrefix = NULL;
    char *tokdes = NULL, *ptokdes = NULL, *pupdtokdes = NULL;
    char *slotdes = NULL, *pslotdes = NULL;
    char *fslotdes = NULL, *ftokdes = NULL;
    char *minPW = NULL;
    index = sftk_argStrip(param);

    PORT_Memset(parsed, 0, sizeof(sftk_parameters));

    while (*index) {
	SFTK_HANDLE_STRING_ARG(index,parsed->configdir,"configDir=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updatedir,"updateDir=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->updateID,"updateID=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->secmodName,"secmod=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->man,"manufacturerID=",;)
	SFTK_HANDLE_STRING_ARG(index,parsed->libdes,"libraryDescription=",;)
	/* constructed values, used so legacy interfaces still work */
	SFTK_HANDLE_STRING_ARG(index,certPrefix,"certPrefix=",;)
        SFTK_HANDLE_STRING_ARG(index,keyPrefix,"keyPrefix=",;)
        SFTK_HANDLE_STRING_ARG(index,tokdes,"cryptoTokenDescription=",;)
        SFTK_HANDLE_STRING_ARG(index,ptokdes,"dbTokenDescription=",;)
        SFTK_HANDLE_STRING_ARG(index,slotdes,"cryptoSlotDescription=",;)
        SFTK_HANDLE_STRING_ARG(index,pslotdes,"dbSlotDescription=",;)
        SFTK_HANDLE_STRING_ARG(index,fslotdes,"FIPSSlotDescription=",;)
        SFTK_HANDLE_STRING_ARG(index,ftokdes,"FIPSTokenDescription=",;)
	SFTK_HANDLE_STRING_ARG(index,pupdtokdes, "updateTokenDescription=",;)
	SFTK_HANDLE_STRING_ARG(index,minPW,"minPWLen=",;)

	SFTK_HANDLE_STRING_ARG(index,tmp,"flags=", 
		if(tmp) { sftk_parseFlags(param,parsed); PORT_Free(tmp); })
	SFTK_HANDLE_STRING_ARG(index,tmp,"tokens=", 
		if(tmp) { sftk_parseTokens(tmp,parsed); PORT_Free(tmp); })
	SFTK_HANDLE_FINAL_ARG(index)
    }
    if (parsed->tokens == NULL) {
	int  count = isFIPS ? 1 : 2;
	int  index = count-1;
	sftk_token_parameters *tokens = NULL;

	tokens = (sftk_token_parameters *) 
			PORT_ZAlloc(count*sizeof(sftk_token_parameters));
	if (tokens == NULL) {
	    goto loser;
	}
	parsed->tokens = tokens;
    	parsed->token_count = count;
	tokens[index].slotID = isFIPS ? FIPS_SLOT_ID : PRIVATE_KEY_SLOT_ID;
	tokens[index].certPrefix = certPrefix;
	tokens[index].keyPrefix = keyPrefix;
	tokens[index].minPW = minPW ? atoi(minPW) : 0;
	tokens[index].readOnly = parsed->readOnly;
	tokens[index].noCertDB = parsed->noCertDB;
	tokens[index].noKeyDB = parsed->noCertDB;
	tokens[index].forceOpen = parsed->forceOpen;
	tokens[index].pwRequired = parsed->pwRequired;
	tokens[index].optimizeSpace = parsed->optimizeSpace;
	tokens[0].optimizeSpace = parsed->optimizeSpace;
	certPrefix = NULL;
	keyPrefix = NULL;
	if (isFIPS) {
	    tokens[index].tokdes = ftokdes;
	    tokens[index].updtokdes = pupdtokdes;
	    tokens[index].slotdes = fslotdes;
	    fslotdes = NULL;
	    ftokdes = NULL;
	    pupdtokdes = NULL;
	} else {
	    tokens[index].tokdes = ptokdes;
	    tokens[index].updtokdes = pupdtokdes;
	    tokens[index].slotdes = pslotdes;
	    tokens[0].slotID = NETSCAPE_SLOT_ID;
	    tokens[0].tokdes = tokdes;
	    tokens[0].slotdes = slotdes;
	    tokens[0].noCertDB = PR_TRUE;
	    tokens[0].noKeyDB = PR_TRUE;
	    pupdtokdes = NULL;
	    ptokdes = NULL;
	    pslotdes = NULL;
	    tokdes = NULL;
	    slotdes = NULL;
	}
    }

loser:
    FREE_CLEAR(certPrefix);
    FREE_CLEAR(keyPrefix);
    FREE_CLEAR(tokdes);
    FREE_CLEAR(ptokdes);
    FREE_CLEAR(pupdtokdes);
    FREE_CLEAR(slotdes);
    FREE_CLEAR(pslotdes);
    FREE_CLEAR(fslotdes);
    FREE_CLEAR(ftokdes);
    FREE_CLEAR(minPW);
    return CKR_OK;
}

void
sftk_freeParams(sftk_parameters *params)
{
    int i;

    for (i=0; i < params->token_count; i++) {
	FREE_CLEAR(params->tokens[i].configdir);
	FREE_CLEAR(params->tokens[i].certPrefix);
	FREE_CLEAR(params->tokens[i].keyPrefix);
	FREE_CLEAR(params->tokens[i].tokdes);
	FREE_CLEAR(params->tokens[i].slotdes);
	FREE_CLEAR(params->tokens[i].updatedir);
	FREE_CLEAR(params->tokens[i].updCertPrefix);
	FREE_CLEAR(params->tokens[i].updKeyPrefix);
	FREE_CLEAR(params->tokens[i].updateID);
	FREE_CLEAR(params->tokens[i].updtokdes);
    }

    FREE_CLEAR(params->configdir);
    FREE_CLEAR(params->secmodName);
    FREE_CLEAR(params->man);
    FREE_CLEAR(params->libdes); 
    FREE_CLEAR(params->tokens);
    FREE_CLEAR(params->updatedir);
    FREE_CLEAR(params->updateID);
}

#define SQLDB "sql:"
#define EXTERNDB "extern:"
#define LEGACY "dbm:"
const char *
sftk_EvaluateConfigDir(const char *configdir, SDBType *dbType, char **appName)
{
    *appName = NULL;
#ifdef NSS_DISABLE_DBM
    *dbType = SDB_SQL;
#else
    *dbType = SDB_LEGACY;
#endif
    if (PORT_Strncmp(configdir, MULTIACCESS, sizeof(MULTIACCESS)-1) == 0) {
	char *cdir;
	*dbType = SDB_MULTIACCESS;

	*appName = PORT_Strdup(configdir+sizeof(MULTIACCESS)-1);
	if (*appName == NULL) {
	    return configdir;
	}
	cdir = *appName;
	while (*cdir && *cdir != ':') {
	    cdir++;
	}
	if (*cdir == ':') {
	   *cdir = 0;
	   cdir++;
	}
	configdir = cdir;
    } else if (PORT_Strncmp(configdir, SQLDB, sizeof(SQLDB)-1) == 0) {
	*dbType = SDB_SQL;
	configdir = configdir + sizeof(SQLDB) -1;
    } else if (PORT_Strncmp(configdir, EXTERNDB, sizeof(EXTERNDB)-1) == 0) {
	*dbType = SDB_EXTERN;
	configdir = configdir + sizeof(EXTERNDB) -1;
    } else if (PORT_Strncmp(configdir, LEGACY, sizeof(LEGACY)-1) == 0) {
	*dbType = SDB_LEGACY;
	configdir = configdir + sizeof(LEGACY) -1;
    } else {
	/* look up the default from the environment */
	char *defaultType = PR_GetEnv("NSS_DEFAULT_DB_TYPE");
	if (defaultType == NULL) {
	    /* none specified, go with the legacy */
	    return configdir;
	}
	if (PORT_Strncmp(defaultType, SQLDB, sizeof(SQLDB)-2) == 0) {
	    *dbType = SDB_SQL;
	} else if (PORT_Strncmp(defaultType,EXTERNDB,sizeof(EXTERNDB)-2)==0) {
	    *dbType = SDB_EXTERN;
	} else if (PORT_Strncmp(defaultType, LEGACY, sizeof(LEGACY)-2) == 0) {
	    *dbType = SDB_LEGACY;
	}
    }
    return configdir;
}

char *
sftk_getSecmodName(char *param, SDBType *dbType, char **appName,
		   char **filename, PRBool *rw)
{
    int next;
    char *configdir = NULL;
    char *secmodName = NULL;
    char *value = NULL;
    char *save_params = param;
    const char *lconfigdir;
    param = sftk_argStrip(param);
	

    while (*param) {
	SFTK_HANDLE_STRING_ARG(param,configdir,"configDir=",;)
	SFTK_HANDLE_STRING_ARG(param,secmodName,"secmod=",;)
	SFTK_HANDLE_FINAL_ARG(param)
   }

   *rw = PR_TRUE;
   if (sftk_argHasFlag("flags","readOnly",save_params)) {
	*rw = PR_FALSE;
   }

   if (!secmodName || *secmodName == '\0') {
	if (secmodName) PORT_Free(secmodName);
	secmodName = PORT_Strdup(SECMOD_DB);
   }

   *filename = secmodName;
   lconfigdir = sftk_EvaluateConfigDir(configdir, dbType, appName);

   if (sftk_argHasFlag("flags","noModDB",save_params)) {
	/* there isn't a module db, don't load the legacy support */
	*dbType = SDB_SQL;
        *rw = PR_FALSE;
   }

   /* only use the renamed secmod for legacy databases */
   if ((*dbType != SDB_LEGACY) && (*dbType != SDB_MULTIACCESS)) {
	secmodName="pkcs11.txt";
   }

   if (lconfigdir) {
	value = PR_smprintf("%s" PATH_SEPARATOR "%s",lconfigdir,secmodName);
   } else {
	value = PR_smprintf("%s",secmodName);
   }
   if (configdir) PORT_Free(configdir);
   return value;
}
