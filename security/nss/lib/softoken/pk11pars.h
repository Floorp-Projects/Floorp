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
 * Portions created by the Initial Developer are Copyright (C) 2001
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
 * The following handles the loading, unloading and management of
 * various PCKS #11 modules
 */


/*
 * this header file contains routines for parsing PKCS #11 module spec
 * strings. It contains 'C' code and should only be included in one module.
 * Currently it is included in both softoken and the wrapper.
 */
#include <ctype.h>
#include "pkcs11.h"
#include "seccomon.h"
#include "prprf.h"
#include "secmodt.h"
#include "pk11init.h"

#define SFTK_ARG_LIBRARY_PARAMETER "library="
#define SFTK_ARG_NAME_PARAMETER "name="
#define SFTK_ARG_MODULE_PARAMETER "parameters="
#define SFTK_ARG_NSS_PARAMETER "NSS="
#define SFTK_ARG_FORTEZZA_FLAG "FORTEZZA"
#define SFTK_ARG_ESCAPE '\\'

struct sftkargSlotFlagTable {
    char *name;
    int len;
    unsigned long value;
};

#define SFTK_DEFAULT_CIPHER_ORDER 0
#define SFTK_DEFAULT_TRUST_ORDER 50


#define SFTK_ARG_ENTRY(arg,flag) \
{ #arg , sizeof(#arg)-1, flag }
static struct sftkargSlotFlagTable sftk_argSlotFlagTable[] = {
	SFTK_ARG_ENTRY(RSA,SECMOD_RSA_FLAG),
	SFTK_ARG_ENTRY(DSA,SECMOD_RSA_FLAG),
	SFTK_ARG_ENTRY(RC2,SECMOD_RC4_FLAG),
	SFTK_ARG_ENTRY(RC4,SECMOD_RC2_FLAG),
	SFTK_ARG_ENTRY(DES,SECMOD_DES_FLAG),
	SFTK_ARG_ENTRY(DH,SECMOD_DH_FLAG),
	SFTK_ARG_ENTRY(FORTEZZA,SECMOD_FORTEZZA_FLAG),
	SFTK_ARG_ENTRY(RC5,SECMOD_RC5_FLAG),
	SFTK_ARG_ENTRY(SHA1,SECMOD_SHA1_FLAG),
	SFTK_ARG_ENTRY(MD5,SECMOD_MD5_FLAG),
	SFTK_ARG_ENTRY(MD2,SECMOD_MD2_FLAG),
	SFTK_ARG_ENTRY(SSL,SECMOD_SSL_FLAG),
	SFTK_ARG_ENTRY(TLS,SECMOD_TLS_FLAG),
	SFTK_ARG_ENTRY(AES,SECMOD_AES_FLAG),
	SFTK_ARG_ENTRY(PublicCerts,SECMOD_FRIENDLY_FLAG),
	SFTK_ARG_ENTRY(RANDOM,SECMOD_RANDOM_FLAG),
};

#define SFTK_HANDLE_STRING_ARG(param,target,value,command) \
    if (PORT_Strncasecmp(param,value,sizeof(value)-1) == 0) { \
	param += sizeof(value)-1; \
	target = sftk_argFetchValue(param,&next); \
	param += next; \
	command ;\
    } else  

#define SFTK_HANDLE_FINAL_ARG(param) \
    { param = sftk_argSkipParameter(param); } param = sftk_argStrip(param);
	

static int sftk_argSlotFlagTableSize = 
	sizeof(sftk_argSlotFlagTable)/sizeof(sftk_argSlotFlagTable[0]);


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
   return isspace(c);
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

static PRBool sftk_argHasChar(char *v, char c)
{
   for ( ;*v; v++) {
	if (*v == c) return PR_TRUE;
   }
   return PR_FALSE;
}

static PRBool sftk_argHasBlanks(char *v)
{
   for ( ;*v; v++) {
	if (sftk_argIsBlank(*v)) return PR_TRUE;
   }
   return PR_FALSE;
}

static char *sftk_argStrip(char *c) {
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

static char *
sftk_argFetchValue(char *string, int *pcount)
{
    char *end = sftk_argFindEnd(string);
    char *retString, *copyString;
    PRBool lastEscape = PR_FALSE;

    *pcount = (end - string)+1;

    if (*pcount == 0) return NULL;

    copyString = retString = (char *)PORT_Alloc(*pcount);
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


static SECStatus
sftk_argParseModuleSpec(char *modulespec, char **lib, char **mod, 
					char **parameters, char **nss)
{
    int next;
    modulespec = sftk_argStrip(modulespec);

    *lib = *mod = *parameters = *nss = 0;

    while (*modulespec) {
	SFTK_HANDLE_STRING_ARG(modulespec,*lib,SFTK_ARG_LIBRARY_PARAMETER,;)
	SFTK_HANDLE_STRING_ARG(modulespec,*mod,SFTK_ARG_NAME_PARAMETER,;)
	SFTK_HANDLE_STRING_ARG(modulespec,*parameters,
						SFTK_ARG_MODULE_PARAMETER,;)
	SFTK_HANDLE_STRING_ARG(modulespec,*nss,SFTK_ARG_NSS_PARAMETER,;)
	SFTK_HANDLE_FINAL_ARG(modulespec)
   }
   return SECSuccess;
}


static char *
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

static void
sftk_argSetNewCipherFlags(unsigned long *newCiphers,char *cipherList)
{
    newCiphers[0] = newCiphers[1] = 0;
    if ((cipherList == NULL) || (*cipherList == 0)) return;

    for (;*cipherList; cipherList=sftk_argNextFlag(cipherList)) {
	if (PORT_Strncasecmp(cipherList,SFTK_ARG_FORTEZZA_FLAG,
				sizeof(SFTK_ARG_FORTEZZA_FLAG)-1) == 0) {
	    newCiphers[0] |= SECMOD_FORTEZZA_FLAG;
	} 

	/* add additional flags here as necessary */
	/* direct bit mapping escape */
	if (*cipherList == 0) {
	   if (cipherList[1] == 'l') {
		newCiphers[1] |= atoi(&cipherList[2]);
	   } else {
		newCiphers[0] |= atoi(&cipherList[2]);
	   }
	}
    }
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

static long
sftk_argReadLong(char *label,char *params, long defValue, PRBool *isdefault)
{
    char *value;
    long retValue;
    if (isdefault) *isdefault = PR_FALSE; 

    value = sftk_argGetParamValue(label,params);
    if (value == NULL) {
	if (isdefault) *isdefault = PR_TRUE;
	return defValue;
    }
    retValue = sftk_argDecodeNumber(value);
    if (value) PORT_Free(value);

    return retValue;
}


static unsigned long
sftk_argSlotFlags(char *label,char *params)
{
    char *flags,*index;
    unsigned long retValue = 0;
    int i;
    PRBool all = PR_FALSE;

    flags = sftk_argGetParamValue(label,params);
    if (flags == NULL) return 0;

    if (PORT_Strcasecmp(flags,"all") == 0) all = PR_TRUE;

    for (index=flags; *index; index=sftk_argNextFlag(index)) {
	for (i=0; i < sftk_argSlotFlagTableSize; i++) {
	    if (all || (PORT_Strncasecmp(index, sftk_argSlotFlagTable[i].name,
				sftk_argSlotFlagTable[i].len) == 0)) {
		retValue |= sftk_argSlotFlagTable[i].value;
	    }
	}
    }
    PORT_Free(flags);
    return retValue;
}


static void
sftk_argDecodeSingleSlotInfo(char *name,char *params,PK11PreSlotInfo *slotInfo)
{
    char *askpw;

    slotInfo->slotID=sftk_argDecodeNumber(name);
    slotInfo->defaultFlags=sftk_argSlotFlags("slotFlags",params);
    slotInfo->timeout=sftk_argReadLong("timeout",params, 0, NULL);

    askpw = sftk_argGetParamValue("askpw",params);
    slotInfo->askpw = 0;

    if (askpw) {
	if (PORT_Strcasecmp(askpw,"every") == 0) {
    	    slotInfo->askpw = -1;
	} else if (PORT_Strcasecmp(askpw,"timeout") == 0) {
    	    slotInfo->askpw = 1;
	} 
	PORT_Free(askpw);
	slotInfo->defaultFlags |= PK11_OWN_PW_DEFAULTS;
    }
    slotInfo->hasRootCerts = sftk_argHasFlag("rootFlags","hasRootCerts",params);
    slotInfo->hasRootTrust = sftk_argHasFlag("rootFlags","hasRootTrust",params);
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

static PK11PreSlotInfo *
sftk_argParseSlotInfo(PRArenaPool *arena, char *slotParams, int *retCount)
{
    char *slotIndex;
    PK11PreSlotInfo *slotInfo = NULL;
    int i=0,count = 0,next;

    *retCount = 0;
    if ((slotParams == NULL) || (*slotParams == 0))  return NULL;

    /* first count the number of slots */
    for (slotIndex = sftk_argStrip(slotParams); *slotIndex; 
		slotIndex = sftk_argStrip(sftk_argSkipParameter(slotIndex))) {
	count++;
    }

    /* get the data structures */
    if (arena) {
	slotInfo = (PK11PreSlotInfo *) 
			PORT_ArenaAlloc(arena,count*sizeof(PK11PreSlotInfo));
	PORT_Memset(slotInfo,0,count*sizeof(PK11PreSlotInfo));
    } else {
	slotInfo = (PK11PreSlotInfo *) 
			PORT_ZAlloc(count*sizeof(PK11PreSlotInfo));
    }
    if (slotInfo == NULL) return NULL;

    for (slotIndex = sftk_argStrip(slotParams), i = 0; 
					*slotIndex && i < count ; ) {
	char *name;
	name = sftk_argGetName(slotIndex,&next);
	slotIndex += next;

	if (!sftk_argIsBlank(*slotIndex)) {
	    char *args = sftk_argFetchValue(slotIndex,&next);
	    slotIndex += next;
	    if (args) {
		sftk_argDecodeSingleSlotInfo(name,args,&slotInfo[i]);
		i++;
		PORT_Free(args);
	    }
	}
	if (name) PORT_Free(name);
	slotIndex = sftk_argStrip(slotIndex);
    }
    *retCount = i;
    return slotInfo;
}

static char *sftk_nullString = "";

static char *
sftk_formatValue(PRArenaPool *arena, char *value, char quote)
{
    char *vp,*vp2,*retval;
    int size = 0, escapes = 0;

    for (vp=value; *vp ;vp++) {
	if ((*vp == quote) || (*vp == SFTK_ARG_ESCAPE)) escapes++;
	size++;
    }
    if (arena) {
	retval = PORT_ArenaZAlloc(arena,size+escapes+1);
    } else {
	retval = PORT_ZAlloc(size+escapes+1);
    }
    if (retval == NULL) return NULL;
    vp2 = retval;
    for (vp=value; *vp; vp++) {
	if ((*vp == quote) || (*vp == SFTK_ARG_ESCAPE)) 
				*vp2++ = SFTK_ARG_ESCAPE;
	*vp2++ = *vp;
    }
    return retval;
}
    
static char *sftk_formatPair(char *name,char *value, char quote)
{
    char openQuote = quote;
    char closeQuote = sftk_argGetPair(quote);
    char *newValue = NULL;
    char *returnValue;
    PRBool need_quote = PR_FALSE;

    if (!value || (*value == 0)) return sftk_nullString;

    if (sftk_argHasBlanks(value) || sftk_argIsQuote(value[0]))
							 need_quote=PR_TRUE;

    if ((need_quote && sftk_argHasChar(value,closeQuote))
				 || sftk_argHasChar(value,SFTK_ARG_ESCAPE)) {
	value = newValue = sftk_formatValue(NULL, value,quote);
	if (newValue == NULL) return sftk_nullString;
    }
    if (need_quote) {
    	returnValue = PR_smprintf("%s=%c%s%c",name,openQuote,value,closeQuote);
    } else {
    	returnValue = PR_smprintf("%s=%s",name,value);
    }
    if (returnValue == NULL) returnValue = sftk_nullString;

    if (newValue) PORT_Free(newValue);

    return returnValue;
}

static char *sftk_formatIntPair(char *name,unsigned long value, unsigned long def)
{
    char *returnValue;

    if (value == def) return sftk_nullString;

    returnValue = PR_smprintf("%s=%d",name,value);

    return returnValue;
}

static void
sftk_freePair(char *pair)
{
    if (pair && pair != sftk_nullString) {
	PR_smprintf_free(pair);
    }
}

#define MAX_FLAG_SIZE  sizeof("internal")+sizeof("FIPS")+sizeof("moduleDB")+\
				sizeof("moduleDBOnly")+sizeof("critical")
static char *
sftk_mkNSSFlags(PRBool internal, PRBool isFIPS,
		PRBool isModuleDB, PRBool isModuleDBOnly, PRBool isCritical)
{
    char *flags = (char *)PORT_ZAlloc(MAX_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags,0,MAX_FLAG_SIZE);
    if (internal) {
	PORT_Strcat(flags,"internal");
	first = PR_FALSE;
    }
    if (isFIPS) {
	if (!first) PORT_Strcat(flags,",");
	PORT_Strcat(flags,"FIPS");
	first = PR_FALSE;
    }
    if (isModuleDB) {
	if (!first) PORT_Strcat(flags,",");
	PORT_Strcat(flags,"moduleDB");
	first = PR_FALSE;
    }
    if (isModuleDBOnly) {
	if (!first) PORT_Strcat(flags,",");
	PORT_Strcat(flags,"moduleDBOnly");
	first = PR_FALSE;
    }
    if (isCritical) {
	if (!first) PORT_Strcat(flags,",");
	PORT_Strcat(flags,"critical");
	first = PR_FALSE;
    }
    return flags;
}

static char *
sftk_mkCipherFlags(unsigned long ssl0, unsigned long ssl1)
{
    char *cipher = NULL;
    int i;

    for (i=0; i < sizeof(ssl0)*8; i++) {
	if (ssl0 & (1<<i)) {
	    char *string;
	    if ((1<<i) == SECMOD_FORTEZZA_FLAG) {
		string = PR_smprintf("%s","FORTEZZA");
	    } else {
		string = PR_smprintf("0h0x%08x",1<<i);
	    }
	    if (cipher) {
		char *tmp;
		tmp = PR_smprintf("%s,%s",cipher,string);
		PR_smprintf_free(cipher);
		PR_smprintf_free(string);
		cipher = tmp;
	    } else {
		cipher = string;
	    }
	}
    }
    for (i=0; i < sizeof(ssl0)*8; i++) {
	if (ssl1 & (1<<i)) {
	    if (cipher) {
		char *tmp;
		tmp = PR_smprintf("%s,0l0x%08x",cipher,1<<i);
		PR_smprintf_free(cipher);
		cipher = tmp;
	    } else {
		cipher = PR_smprintf("0l0x%08x",1<<i);
	    }
	}
    }

    return cipher;
}

static char *
sftk_mkSlotFlags(unsigned long defaultFlags)
{
    char *flags=NULL;
    int i,j;

    for (i=0; i < sizeof(defaultFlags)*8; i++) {
	if (defaultFlags & (1<<i)) {
	    char *string = NULL;

	    for (j=0; j < sftk_argSlotFlagTableSize; j++) {
		if (sftk_argSlotFlagTable[j].value == (((unsigned long)1)<<i)) {
		    string = sftk_argSlotFlagTable[j].name;
		    break;
		}
	    }
	    if (string) {
		if (flags) {
		    char *tmp;
		    tmp = PR_smprintf("%s,%s",flags,string);
		    PR_smprintf_free(flags);
		    flags = tmp;
		} else {
		    flags = PR_smprintf("%s",string);
		}
	    }
	}
    }

    return flags;
}

#define SFTK_MAX_ROOT_FLAG_SIZE  sizeof("hasRootCerts")+sizeof("hasRootTrust")

static char *
sftk_mkRootFlags(PRBool hasRootCerts, PRBool hasRootTrust)
{
    char *flags= (char *)PORT_ZAlloc(SFTK_MAX_ROOT_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags,0,SFTK_MAX_ROOT_FLAG_SIZE);
    if (hasRootCerts) {
	PORT_Strcat(flags,"hasRootCerts");
	first = PR_FALSE;
    }
    if (hasRootTrust) {
	if (!first) PORT_Strcat(flags,",");
	PORT_Strcat(flags,"hasRootTrust");
	first = PR_FALSE;
    }
    return flags;
}

static char *
sftk_mkSlotString(unsigned long slotID, unsigned long defaultFlags,
		  unsigned long timeout, unsigned char askpw_in,
		  PRBool hasRootCerts, PRBool hasRootTrust) {
    char *askpw,*flags,*rootFlags,*slotString;
    char *flagPair,*rootFlagsPair;
	
    switch (askpw_in) {
    case 0xff:
	askpw = "every";
	break;
    case 1:
	askpw = "timeout";
	break;
    default:
	askpw = "any";
	break;
    }
    flags = sftk_mkSlotFlags(defaultFlags);
    rootFlags = sftk_mkRootFlags(hasRootCerts,hasRootTrust);
    flagPair=sftk_formatPair("slotFlags",flags,'\'');
    rootFlagsPair=sftk_formatPair("rootFlags",rootFlags,'\'');
    if (flags) PR_smprintf_free(flags);
    if (rootFlags) PORT_Free(rootFlags);
    if (defaultFlags & PK11_OWN_PW_DEFAULTS) {
    	slotString = PR_smprintf("0x%08lx=[%s askpw=%s timeout=%d %s]",
				(PRUint32)slotID,flagPair,askpw,timeout,
				rootFlagsPair);
    } else {
    	slotString = PR_smprintf("0x%08lx=[%s %s]",
				(PRUint32)slotID,flagPair,rootFlagsPair);
    }
    sftk_freePair(flagPair);
    sftk_freePair(rootFlagsPair);
    return slotString;
}

static char *
sftk_mkNSS(char **slotStrings, int slotCount, PRBool internal, PRBool isFIPS,
	  PRBool isModuleDB,  PRBool isModuleDBOnly, PRBool isCritical, 
	  unsigned long trustOrder, unsigned long cipherOrder,
				unsigned long ssl0, unsigned long ssl1) {
    int slotLen, i;
    char *slotParams, *ciphers, *nss, *nssFlags, *tmp;
    char *trustOrderPair,*cipherOrderPair,*slotPair,*cipherPair,*flagPair;


    /* now let's build up the string
     * first the slot infos
     */
    slotLen=0;
    for (i=0; i < (int)slotCount; i++) {
	slotLen += PORT_Strlen(slotStrings[i])+1;
    }
    slotLen += 1; /* space for the final NULL */

    slotParams = (char *)PORT_ZAlloc(slotLen);
    PORT_Memset(slotParams,0,slotLen);
    for (i=0; i < (int)slotCount; i++) {
	PORT_Strcat(slotParams,slotStrings[i]);
	PORT_Strcat(slotParams," ");
	PR_smprintf_free(slotStrings[i]);
	slotStrings[i]=NULL;
    }
    
    /*
     * now the NSS structure
     */
    nssFlags = sftk_mkNSSFlags(internal,isFIPS,isModuleDB,isModuleDBOnly,
							isCritical); 
	/* for now only the internal module is critical */
    ciphers = sftk_mkCipherFlags(ssl0, ssl1);

    trustOrderPair=sftk_formatIntPair("trustOrder",trustOrder,
					SFTK_DEFAULT_TRUST_ORDER);
    cipherOrderPair=sftk_formatIntPair("cipherOrder",cipherOrder,
					SFTK_DEFAULT_CIPHER_ORDER);
    slotPair=sftk_formatPair("slotParams",slotParams,'{'); /* } */
    if (slotParams) PORT_Free(slotParams);
    cipherPair=sftk_formatPair("ciphers",ciphers,'\'');
    if (ciphers) PR_smprintf_free(ciphers);
    flagPair=sftk_formatPair("Flags",nssFlags,'\'');
    if (nssFlags) PORT_Free(nssFlags);
    nss = PR_smprintf("%s %s %s %s %s",trustOrderPair,
			cipherOrderPair,slotPair,cipherPair,flagPair);
    sftk_freePair(trustOrderPair);
    sftk_freePair(cipherOrderPair);
    sftk_freePair(slotPair);
    sftk_freePair(cipherPair);
    sftk_freePair(flagPair);
    tmp = sftk_argStrip(nss);
    if (*tmp == '\0') {
	PR_smprintf_free(nss);
	nss = NULL;
    }
    return nss;
}

static char *
sftk_mkNewModuleSpec(char *dllName, char *commonName, char *parameters, 
								char *NSS) {
    char *moduleSpec;
    char *lib,*name,*param,*nss;

    /*
     * now the final spec
     */
    lib = sftk_formatPair("library",dllName,'\"');
    name = sftk_formatPair("name",commonName,'\"');
    param = sftk_formatPair("parameters",parameters,'\"');
    nss = sftk_formatPair("NSS",NSS,'\"');
    moduleSpec = PR_smprintf("%s %s %s %s", lib,name,param,nss);
    sftk_freePair(lib);
    sftk_freePair(name);
    sftk_freePair(param);
    sftk_freePair(nss);
    return (moduleSpec);
}

