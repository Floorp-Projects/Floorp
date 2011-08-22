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

#define SECMOD_ARG_LIBRARY_PARAMETER "library="
#define SECMOD_ARG_NAME_PARAMETER "name="
#define SECMOD_ARG_MODULE_PARAMETER "parameters="
#define SECMOD_ARG_NSS_PARAMETER "NSS="
#define SECMOD_ARG_FORTEZZA_FLAG "FORTEZZA"
#define SECMOD_ARG_ESCAPE '\\'

struct secmodargSlotFlagTable {
    char *name;
    int len;
    unsigned long value;
};

#define SECMOD_DEFAULT_CIPHER_ORDER 0
#define SECMOD_DEFAULT_TRUST_ORDER 50


#define SECMOD_ARG_ENTRY(arg,flag) \
{ #arg , sizeof(#arg)-1, flag }
static struct secmodargSlotFlagTable secmod_argSlotFlagTable[] = {
	SECMOD_ARG_ENTRY(RSA,SECMOD_RSA_FLAG),
	SECMOD_ARG_ENTRY(DSA,SECMOD_RSA_FLAG),
	SECMOD_ARG_ENTRY(RC2,SECMOD_RC4_FLAG),
	SECMOD_ARG_ENTRY(RC4,SECMOD_RC2_FLAG),
	SECMOD_ARG_ENTRY(DES,SECMOD_DES_FLAG),
	SECMOD_ARG_ENTRY(DH,SECMOD_DH_FLAG),
	SECMOD_ARG_ENTRY(FORTEZZA,SECMOD_FORTEZZA_FLAG),
	SECMOD_ARG_ENTRY(RC5,SECMOD_RC5_FLAG),
	SECMOD_ARG_ENTRY(SHA1,SECMOD_SHA1_FLAG),
	SECMOD_ARG_ENTRY(MD5,SECMOD_MD5_FLAG),
	SECMOD_ARG_ENTRY(MD2,SECMOD_MD2_FLAG),
	SECMOD_ARG_ENTRY(SSL,SECMOD_SSL_FLAG),
	SECMOD_ARG_ENTRY(TLS,SECMOD_TLS_FLAG),
	SECMOD_ARG_ENTRY(AES,SECMOD_AES_FLAG),
	SECMOD_ARG_ENTRY(Camellia,SECMOD_CAMELLIA_FLAG),
	SECMOD_ARG_ENTRY(SEED,SECMOD_SEED_FLAG),
	SECMOD_ARG_ENTRY(PublicCerts,SECMOD_FRIENDLY_FLAG),
	SECMOD_ARG_ENTRY(RANDOM,SECMOD_RANDOM_FLAG),
	SECMOD_ARG_ENTRY(Disable, PK11_DISABLE_FLAG),
};

#define SECMOD_HANDLE_STRING_ARG(param,target,value,command) \
    if (PORT_Strncasecmp(param,value,sizeof(value)-1) == 0) { \
	param += sizeof(value)-1; \
	if (target) PORT_Free(target); \
	target = secmod_argFetchValue(param,&next); \
	param += next; \
	command ;\
    } else  

#define SECMOD_HANDLE_FINAL_ARG(param) \
    { param = secmod_argSkipParameter(param); } param = secmod_argStrip(param);
	

static int secmod_argSlotFlagTableSize = 
	sizeof(secmod_argSlotFlagTable)/sizeof(secmod_argSlotFlagTable[0]);


static PRBool secmod_argGetPair(char c) {
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

static PRBool secmod_argIsBlank(char c) {
   return isspace((unsigned char )c);
}

static PRBool secmod_argIsEscape(char c) {
    return c == '\\';
}

static PRBool secmod_argIsQuote(char c) {
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

static PRBool secmod_argHasChar(char *v, char c)
{
   for ( ;*v; v++) {
	if (*v == c) return PR_TRUE;
   }
   return PR_FALSE;
}

static PRBool secmod_argHasBlanks(char *v)
{
   for ( ;*v; v++) {
	if (secmod_argIsBlank(*v)) return PR_TRUE;
   }
   return PR_FALSE;
}

static char *secmod_argStrip(char *c) {
   while (*c && secmod_argIsBlank(*c)) c++;
   return c;
}

static char *
secmod_argFindEnd(char *string) {
    char endChar = ' ';
    PRBool lastEscape = PR_FALSE;

    if (secmod_argIsQuote(*string)) {
	endChar = secmod_argGetPair(*string);
	string++;
    }

    for (;*string; string++) {
	if (lastEscape) {
	    lastEscape = PR_FALSE;
	    continue;
	}
	if (secmod_argIsEscape(*string) && !lastEscape) {
	    lastEscape = PR_TRUE;
	    continue;
	} 
	if ((endChar == ' ') && secmod_argIsBlank(*string)) break;
	if (*string == endChar) {
	    break;
	}
    }

    return string;
}

static char *
secmod_argFetchValue(char *string, int *pcount)
{
    char *end = secmod_argFindEnd(string);
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


    if (secmod_argIsQuote(*string)) string++;
    for (; string < end; string++) {
	if (secmod_argIsEscape(*string) && !lastEscape) {
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
secmod_argSkipParameter(char *string) 
{
     char *end;
     /* look for the end of the <name>= */
     for (;*string; string++) {
	if (*string == '=') { string++; break; }
	if (secmod_argIsBlank(*string)) return(string); 
     }

     end = secmod_argFindEnd(string);
     if (*end) end++;
     return end;
}


static SECStatus
secmod_argParseModuleSpec(char *modulespec, char **lib, char **mod, 
					char **parameters, char **nss)
{
    int next;
    modulespec = secmod_argStrip(modulespec);

    *lib = *mod = *parameters = *nss = 0;

    while (*modulespec) {
	SECMOD_HANDLE_STRING_ARG(modulespec,*lib,SECMOD_ARG_LIBRARY_PARAMETER,;)
	SECMOD_HANDLE_STRING_ARG(modulespec,*mod,SECMOD_ARG_NAME_PARAMETER,;)
	SECMOD_HANDLE_STRING_ARG(modulespec,*parameters,
						SECMOD_ARG_MODULE_PARAMETER,;)
	SECMOD_HANDLE_STRING_ARG(modulespec,*nss,SECMOD_ARG_NSS_PARAMETER,;)
	SECMOD_HANDLE_FINAL_ARG(modulespec)
   }
   return SECSuccess;
}


static char *
secmod_argGetParamValue(char *paramName,char *parameters)
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
	    returnValue = secmod_argFetchValue(parameters,&next);
	    break;
	} else {
	    parameters = secmod_argSkipParameter(parameters);
	}
	parameters = secmod_argStrip(parameters);
   }
   return returnValue;
}
    

static char *
secmod_argNextFlag(char *flags)
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
secmod_argHasFlag(char *label, char *flag, char *parameters)
{
    char *flags,*index;
    int len = strlen(flag);
    PRBool found = PR_FALSE;

    flags = secmod_argGetParamValue(label,parameters);
    if (flags == NULL) return PR_FALSE;

    for (index=flags; *index; index=secmod_argNextFlag(index)) {
	if (PORT_Strncasecmp(index,flag,len) == 0) {
	    found=PR_TRUE;
	    break;
	}
    }
    PORT_Free(flags);
    return found;
}

static void
secmod_argSetNewCipherFlags(unsigned long *newCiphers,char *cipherList)
{
    newCiphers[0] = newCiphers[1] = 0;
    if ((cipherList == NULL) || (*cipherList == 0)) return;

    for (;*cipherList; cipherList=secmod_argNextFlag(cipherList)) {
	if (PORT_Strncasecmp(cipherList,SECMOD_ARG_FORTEZZA_FLAG,
				sizeof(SECMOD_ARG_FORTEZZA_FLAG)-1) == 0) {
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
secmod_argDecodeNumber(char *num)
{
    int	radix = 10;
    unsigned long value = 0;
    long retValue = 0;
    int sign = 1;
    int digit;

    if (num == NULL) return retValue;

    num = secmod_argStrip(num);

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
secmod_argReadLong(char *label,char *params, long defValue, PRBool *isdefault)
{
    char *value;
    long retValue;
    if (isdefault) *isdefault = PR_FALSE; 

    value = secmod_argGetParamValue(label,params);
    if (value == NULL) {
	if (isdefault) *isdefault = PR_TRUE;
	return defValue;
    }
    retValue = secmod_argDecodeNumber(value);
    if (value) PORT_Free(value);

    return retValue;
}


static unsigned long
secmod_argSlotFlags(char *label,char *params)
{
    char *flags,*index;
    unsigned long retValue = 0;
    int i;
    PRBool all = PR_FALSE;

    flags = secmod_argGetParamValue(label,params);
    if (flags == NULL) return 0;

    if (PORT_Strcasecmp(flags,"all") == 0) all = PR_TRUE;

    for (index=flags; *index; index=secmod_argNextFlag(index)) {
	for (i=0; i < secmod_argSlotFlagTableSize; i++) {
	    if (all || (PORT_Strncasecmp(index, secmod_argSlotFlagTable[i].name,
				secmod_argSlotFlagTable[i].len) == 0)) {
		retValue |= secmod_argSlotFlagTable[i].value;
	    }
	}
    }
    PORT_Free(flags);
    return retValue;
}


static void
secmod_argDecodeSingleSlotInfo(char *name, char *params, 
                               PK11PreSlotInfo *slotInfo)
{
    char *askpw;

    slotInfo->slotID=secmod_argDecodeNumber(name);
    slotInfo->defaultFlags=secmod_argSlotFlags("slotFlags",params);
    slotInfo->timeout=secmod_argReadLong("timeout",params, 0, NULL);

    askpw = secmod_argGetParamValue("askpw",params);
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
    slotInfo->hasRootCerts = secmod_argHasFlag("rootFlags", "hasRootCerts", 
                                               params);
    slotInfo->hasRootTrust = secmod_argHasFlag("rootFlags", "hasRootTrust", 
                                               params);
}

static char *
secmod_argGetName(char *inString, int *next) 
{
    char *name=NULL;
    char *string;
    int len;

    /* look for the end of the <name>= */
    for (string = inString;*string; string++) {
	if (*string == '=') { break; }
	if (secmod_argIsBlank(*string)) break;
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
secmod_argParseSlotInfo(PRArenaPool *arena, char *slotParams, int *retCount)
{
    char *slotIndex;
    PK11PreSlotInfo *slotInfo = NULL;
    int i=0,count = 0,next;

    *retCount = 0;
    if ((slotParams == NULL) || (*slotParams == 0))  return NULL;

    /* first count the number of slots */
    for (slotIndex = secmod_argStrip(slotParams); *slotIndex; 
	 slotIndex = secmod_argStrip(secmod_argSkipParameter(slotIndex))) {
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

    for (slotIndex = secmod_argStrip(slotParams), i = 0; 
					*slotIndex && i < count ; ) {
	char *name;
	name = secmod_argGetName(slotIndex,&next);
	slotIndex += next;

	if (!secmod_argIsBlank(*slotIndex)) {
	    char *args = secmod_argFetchValue(slotIndex,&next);
	    slotIndex += next;
	    if (args) {
		secmod_argDecodeSingleSlotInfo(name,args,&slotInfo[i]);
		i++;
		PORT_Free(args);
	    }
	}
	if (name) PORT_Free(name);
	slotIndex = secmod_argStrip(slotIndex);
    }
    *retCount = i;
    return slotInfo;
}

static char *secmod_nullString = "";

static char *
secmod_formatValue(PRArenaPool *arena, char *value, char quote)
{
    char *vp,*vp2,*retval;
    int size = 0, escapes = 0;

    for (vp=value; *vp ;vp++) {
	if ((*vp == quote) || (*vp == SECMOD_ARG_ESCAPE)) escapes++;
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
	if ((*vp == quote) || (*vp == SECMOD_ARG_ESCAPE)) 
				*vp2++ = SECMOD_ARG_ESCAPE;
	*vp2++ = *vp;
    }
    return retval;
}
    
static char *secmod_formatPair(char *name,char *value, char quote)
{
    char openQuote = quote;
    char closeQuote = secmod_argGetPair(quote);
    char *newValue = NULL;
    char *returnValue;
    PRBool need_quote = PR_FALSE;

    if (!value || (*value == 0)) return secmod_nullString;

    if (secmod_argHasBlanks(value) || secmod_argIsQuote(value[0]))
							 need_quote=PR_TRUE;

    if ((need_quote && secmod_argHasChar(value,closeQuote))
				 || secmod_argHasChar(value,SECMOD_ARG_ESCAPE)) {
	value = newValue = secmod_formatValue(NULL, value,quote);
	if (newValue == NULL) return secmod_nullString;
    }
    if (need_quote) {
    	returnValue = PR_smprintf("%s=%c%s%c",name,openQuote,value,closeQuote);
    } else {
    	returnValue = PR_smprintf("%s=%s",name,value);
    }
    if (returnValue == NULL) returnValue = secmod_nullString;

    if (newValue) PORT_Free(newValue);

    return returnValue;
}

static char *secmod_formatIntPair(char *name, unsigned long value, 
                                  unsigned long def)
{
    char *returnValue;

    if (value == def) return secmod_nullString;

    returnValue = PR_smprintf("%s=%d",name,value);

    return returnValue;
}

static void
secmod_freePair(char *pair)
{
    if (pair && pair != secmod_nullString) {
	PR_smprintf_free(pair);
    }
}

#define MAX_FLAG_SIZE  sizeof("internal")+sizeof("FIPS")+sizeof("moduleDB")+\
				sizeof("moduleDBOnly")+sizeof("critical")
static char *
secmod_mkNSSFlags(PRBool internal, PRBool isFIPS,
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
secmod_mkCipherFlags(unsigned long ssl0, unsigned long ssl1)
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
secmod_mkSlotFlags(unsigned long defaultFlags)
{
    char *flags=NULL;
    int i,j;

    for (i=0; i < sizeof(defaultFlags)*8; i++) {
	if (defaultFlags & (1<<i)) {
	    char *string = NULL;

	    for (j=0; j < secmod_argSlotFlagTableSize; j++) {
		if (secmod_argSlotFlagTable[j].value == ( 1UL << i )) {
		    string = secmod_argSlotFlagTable[j].name;
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

#define SECMOD_MAX_ROOT_FLAG_SIZE  sizeof("hasRootCerts")+sizeof("hasRootTrust")

static char *
secmod_mkRootFlags(PRBool hasRootCerts, PRBool hasRootTrust)
{
    char *flags= (char *)PORT_ZAlloc(SECMOD_MAX_ROOT_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags,0,SECMOD_MAX_ROOT_FLAG_SIZE);
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
secmod_mkSlotString(unsigned long slotID, unsigned long defaultFlags,
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
    flags = secmod_mkSlotFlags(defaultFlags);
    rootFlags = secmod_mkRootFlags(hasRootCerts,hasRootTrust);
    flagPair=secmod_formatPair("slotFlags",flags,'\'');
    rootFlagsPair=secmod_formatPair("rootFlags",rootFlags,'\'');
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
    secmod_freePair(flagPair);
    secmod_freePair(rootFlagsPair);
    return slotString;
}

static char *
secmod_mkNSS(char **slotStrings, int slotCount, PRBool internal, PRBool isFIPS,
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
    nssFlags = secmod_mkNSSFlags(internal,isFIPS,isModuleDB,isModuleDBOnly,
							isCritical); 
	/* for now only the internal module is critical */
    ciphers = secmod_mkCipherFlags(ssl0, ssl1);

    trustOrderPair=secmod_formatIntPair("trustOrder",trustOrder,
					SECMOD_DEFAULT_TRUST_ORDER);
    cipherOrderPair=secmod_formatIntPair("cipherOrder",cipherOrder,
					SECMOD_DEFAULT_CIPHER_ORDER);
    slotPair=secmod_formatPair("slotParams",slotParams,'{'); /* } */
    if (slotParams) PORT_Free(slotParams);
    cipherPair=secmod_formatPair("ciphers",ciphers,'\'');
    if (ciphers) PR_smprintf_free(ciphers);
    flagPair=secmod_formatPair("Flags",nssFlags,'\'');
    if (nssFlags) PORT_Free(nssFlags);
    nss = PR_smprintf("%s %s %s %s %s",trustOrderPair,
			cipherOrderPair,slotPair,cipherPair,flagPair);
    secmod_freePair(trustOrderPair);
    secmod_freePair(cipherOrderPair);
    secmod_freePair(slotPair);
    secmod_freePair(cipherPair);
    secmod_freePair(flagPair);
    tmp = secmod_argStrip(nss);
    if (*tmp == '\0') {
	PR_smprintf_free(nss);
	nss = NULL;
    }
    return nss;
}

static char *
secmod_mkNewModuleSpec(char *dllName, char *commonName, char *parameters, 
								char *NSS) {
    char *moduleSpec;
    char *lib,*name,*param,*nss;

    /*
     * now the final spec
     */
    lib = secmod_formatPair("library",dllName,'\"');
    name = secmod_formatPair("name",commonName,'\"');
    param = secmod_formatPair("parameters",parameters,'\"');
    nss = secmod_formatPair("NSS",NSS,'\"');
    moduleSpec = PR_smprintf("%s %s %s %s", lib,name,param,nss);
    secmod_freePair(lib);
    secmod_freePair(name);
    secmod_freePair(param);
    secmod_freePair(nss);
    return (moduleSpec);
}

