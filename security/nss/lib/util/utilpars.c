/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* 
 * The following code handles the storage of PKCS 11 modules used by the
 * NSS. This file is written to abstract away how the modules are
 * stored so we can decide that later.
 */
#include "secport.h"
#include "prprf.h" 
#include "prenv.h"
#include "utilpars.h"
#include "utilmodt.h"

/*
 * return the expected matching quote value for the one specified
 */
PRBool NSSUTIL_ArgGetPair(char c) {
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

PRBool NSSUTIL_ArgIsBlank(char c) {
   return isspace((unsigned char )c);
}

PRBool NSSUTIL_ArgIsEscape(char c) {
    return c == '\\';
}

PRBool NSSUTIL_ArgIsQuote(char c) {
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

char *NSSUTIL_ArgStrip(char *c) {
   while (*c && NSSUTIL_ArgIsBlank(*c)) c++;
   return c;
}

/*
 * find the end of the current tag/value pair. string should be pointing just
 * after the equal sign. Handles quoted characters.
 */
char *
NSSUTIL_ArgFindEnd(char *string) {
    char endChar = ' ';
    PRBool lastEscape = PR_FALSE;

    if (NSSUTIL_ArgIsQuote(*string)) {
	endChar = NSSUTIL_ArgGetPair(*string);
	string++;
    }

    for (;*string; string++) {
	if (lastEscape) {
	    lastEscape = PR_FALSE;
	    continue;
	}
	if (NSSUTIL_ArgIsEscape(*string) && !lastEscape) {
	    lastEscape = PR_TRUE;
	    continue;
	} 
	if ((endChar == ' ') && NSSUTIL_ArgIsBlank(*string)) break;
	if (*string == endChar) {
	    break;
	}
    }

    return string;
}

/*
 * get the value pointed to by string. string should be pointing just beyond
 * the equal sign.
 */
char *
NSSUTIL_ArgFetchValue(char *string, int *pcount)
{
    char *end = NSSUTIL_ArgFindEnd(string);
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


    if (NSSUTIL_ArgIsQuote(*string)) string++;
    for (; string < end; string++) {
	if (NSSUTIL_ArgIsEscape(*string) && !lastEscape) {
	    lastEscape = PR_TRUE;
	    continue;
	}
	lastEscape = PR_FALSE;
	*copyString++ = *string;
    }
    *copyString = 0;
    return retString;
}

/*
 * point to the next parameter in string
 */
char *
NSSUTIL_ArgSkipParameter(char *string) 
{
     char *end;
     /* look for the end of the <name>= */
     for (;*string; string++) {
	if (*string == '=') { string++; break; }
	if (NSSUTIL_ArgIsBlank(*string)) return(string); 
     }

     end = NSSUTIL_ArgFindEnd(string);
     if (*end) end++;
     return end;
}

/*
 * get the value from that tag value pair.
 */
char *
NSSUTIL_ArgGetParamValue(char *paramName,char *parameters)
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
	    returnValue = NSSUTIL_ArgFetchValue(parameters,&next);
	    break;
	} else {
	    parameters = NSSUTIL_ArgSkipParameter(parameters);
	}
	parameters = NSSUTIL_ArgStrip(parameters);
   }
   return returnValue;
}
  
/*
 * find the next flag in the parameter list
 */  
char *
NSSUTIL_ArgNextFlag(char *flags)
{
    for (; *flags ; flags++) {
	if (*flags == ',') {
	    flags++;
	    break;
	}
    }
    return flags;
}

/*
 * return true if the flag is set in the label parameter.
 */
PRBool
NSSUTIL_ArgHasFlag(char *label, char *flag, char *parameters)
{
    char *flags,*index;
    int len = strlen(flag);
    PRBool found = PR_FALSE;

    flags = NSSUTIL_ArgGetParamValue(label,parameters);
    if (flags == NULL) return PR_FALSE;

    for (index=flags; *index; index=NSSUTIL_ArgNextFlag(index)) {
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
long
NSSUTIL_ArgDecodeNumber(char *num)
{
    int	radix = 10;
    unsigned long value = 0;
    long retValue = 0;
    int sign = 1;
    int digit;

    if (num == NULL) return retValue;

    num = NSSUTIL_ArgStrip(num);

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

/*
 * parameters are tag value pairs. This function returns the tag or label (the
 * value before the equal size.
 */
char *
NSSUTIL_ArgGetLabel(char *inString, int *next) 
{
    char *name=NULL;
    char *string;
    int len;

    /* look for the end of the <label>= */
    for (string = inString;*string; string++) {
	if (*string == '=') { break; }
	if (NSSUTIL_ArgIsBlank(*string)) break;
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

/*
 * read an argument at a Long integer
 */
long
NSSUTIL_ArgReadLong(char *label,char *params, long defValue, PRBool *isdefault)
{
    char *value;
    long retValue;
    if (isdefault) *isdefault = PR_FALSE; 

    value = NSSUTIL_ArgGetParamValue(label,params);
    if (value == NULL) {
	if (isdefault) *isdefault = PR_TRUE;
	return defValue;
    }
    retValue = NSSUTIL_ArgDecodeNumber(value);
    if (value) PORT_Free(value);

    return retValue;
}


/*
 * prepare a string to be quoted with 'quote' marks. We do that by adding
 * appropriate escapes.
 */
static int
nssutil_escapeQuotesSize(const char *string, char quote, PRBool addquotes)
{
    int escapes = 0, size = 0;
    const char *src;

    size= addquotes ? 2 : 0;
    for (src=string; *src ; src++) {
	if ((*src == quote) || (*src == '\\')) escapes++;
	size++;
    }
    return size+escapes+1;

}

static char *
nssutil_escapeQuotes(const char *string, char quote, PRBool addquotes)
{
    char *newString = 0;
    int size = 0;
    const char *src;
    char *dest;

    size = nssutil_escapeQuotesSize(string, quote, addquotes);

    dest = newString = PORT_ZAlloc(size); 
    if (newString == NULL) {
	return NULL;
    }

    if (addquotes) *dest++=quote;
    for (src=string; *src; src++,dest++) {
	if ((*src == '\\') || (*src == quote)) {
	    *dest++ = '\\';
	}
	*dest = *src;
    }
    if (addquotes) *dest=quote;

    return newString;
}

int
NSSUTIL_EscapeSize(const char *string, char quote)
{
     return nssutil_escapeQuotesSize(string, quote, PR_FALSE);
}

char *
NSSUTIL_Escape(const char *string, char quote)
{
    return nssutil_escapeQuotes(string, quote, PR_FALSE);
}


int
NSSUTIL_QuoteSize(const char *string, char quote)
{
     return nssutil_escapeQuotesSize(string, quote, PR_TRUE);
}

char *
NSSUTIL_Quote(const char *string, char quote)
{
    return nssutil_escapeQuotes(string, quote, PR_TRUE);
}

int
NSSUTIL_DoubleEscapeSize(const char *string, char quote1, char quote2)
{
    int escapes = 0, size = 0;
    const char *src;
    for (src=string; *src ; src++) {
        if (*src == '\\')   escapes+=3; /* \\\\ */
        if (*src == quote1) escapes+=2; /* \\quote1 */
        if (*src == quote2) escapes++;   /* \quote2 */
        size++;
    }

    return escapes+size+1;
}

char *
NSSUTIL_DoubleEscape(const char *string, char quote1, char quote2)
{
    char *round1 = NULL;
    char *retValue = NULL;
    if (string == NULL) {
        goto done;
    }
    round1 = nssutil_escapeQuotes(string, quote1, PR_FALSE);
    if (round1) {
        retValue = nssutil_escapeQuotes(round1, quote2, PR_FALSE);
        PORT_Free(round1);
    }

done:
    if (retValue == NULL) {
        retValue = PORT_Strdup("");
    }
    return retValue;
}


/************************************************************************
 * These functions are used in contructing strings.
 * NOTE: they will always return a string, but sometimes it will return
 * a specific NULL string. These strings must be freed with util_freePair.
 */

/* string to return on error... */
static char *nssutil_nullString = "";

static char *
nssutil_formatValue(PLArenaPool *arena, char *value, char quote)
{
    char *vp,*vp2,*retval;
    int size = 0, escapes = 0;

    for (vp=value; *vp ;vp++) {
	if ((*vp == quote) || (*vp == NSSUTIL_ARG_ESCAPE)) escapes++;
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
	if ((*vp == quote) || (*vp == NSSUTIL_ARG_ESCAPE)) 
				*vp2++ = NSSUTIL_ARG_ESCAPE;
	*vp2++ = *vp;
    }
    return retval;
}


static PRBool nssutil_argHasChar(char *v, char c)
{
   for ( ;*v; v++) {
        if (*v == c) return PR_TRUE;
   }
   return PR_FALSE;
}

static PRBool nssutil_argHasBlanks(char *v)
{
   for ( ;*v; v++) {
        if (NSSUTIL_ArgIsBlank(*v)) return PR_TRUE;
   }
   return PR_FALSE;
}

static char *
nssutil_formatPair(char *name, char *value, char quote)
{
    char openQuote = quote;
    char closeQuote = NSSUTIL_ArgGetPair(quote);
    char *newValue = NULL;
    char *returnValue;
    PRBool need_quote = PR_FALSE;

    if (!value || (*value == 0)) return nssutil_nullString;

    if (nssutil_argHasBlanks(value) || NSSUTIL_ArgIsQuote(value[0]))
							 need_quote=PR_TRUE;

    if ((need_quote && nssutil_argHasChar(value,closeQuote))
			|| nssutil_argHasChar(value,NSSUTIL_ARG_ESCAPE)) {
	value = newValue = nssutil_formatValue(NULL, value,quote);
	if (newValue == NULL) return nssutil_nullString;
    }
    if (need_quote) {
    	returnValue = PR_smprintf("%s=%c%s%c",name,openQuote,value,closeQuote);
    } else {
    	returnValue = PR_smprintf("%s=%s",name,value);
    }
    if (returnValue == NULL) returnValue = nssutil_nullString;

    if (newValue) PORT_Free(newValue);

    return returnValue;
}

static char *nssutil_formatIntPair(char *name, unsigned long value, 
                                  unsigned long def)
{
    char *returnValue;

    if (value == def) return nssutil_nullString;

    returnValue = PR_smprintf("%s=%d",name,value);

    return returnValue;
}

static void
nssutil_freePair(char *pair)
{
    if (pair && pair != nssutil_nullString) {
	PR_smprintf_free(pair);
    }
}


/************************************************************************
 * Parse the Slot specific parameters in the NSS params.
 */

struct nssutilArgSlotFlagTable {
    char *name;
    int len;
    unsigned long value;
};

#define NSSUTIL_ARG_ENTRY(arg,flag) \
{ #arg , sizeof(#arg)-1, flag }
static struct nssutilArgSlotFlagTable nssutil_argSlotFlagTable[] = {
	NSSUTIL_ARG_ENTRY(RSA,SECMOD_RSA_FLAG),
	NSSUTIL_ARG_ENTRY(DSA,SECMOD_RSA_FLAG),
	NSSUTIL_ARG_ENTRY(RC2,SECMOD_RC4_FLAG),
	NSSUTIL_ARG_ENTRY(RC4,SECMOD_RC2_FLAG),
	NSSUTIL_ARG_ENTRY(DES,SECMOD_DES_FLAG),
	NSSUTIL_ARG_ENTRY(DH,SECMOD_DH_FLAG),
	NSSUTIL_ARG_ENTRY(FORTEZZA,SECMOD_FORTEZZA_FLAG),
	NSSUTIL_ARG_ENTRY(RC5,SECMOD_RC5_FLAG),
	NSSUTIL_ARG_ENTRY(SHA1,SECMOD_SHA1_FLAG),
	NSSUTIL_ARG_ENTRY(SHA256,SECMOD_SHA256_FLAG),
	NSSUTIL_ARG_ENTRY(SHA512,SECMOD_SHA512_FLAG),
	NSSUTIL_ARG_ENTRY(MD5,SECMOD_MD5_FLAG),
	NSSUTIL_ARG_ENTRY(MD2,SECMOD_MD2_FLAG),
	NSSUTIL_ARG_ENTRY(SSL,SECMOD_SSL_FLAG),
	NSSUTIL_ARG_ENTRY(TLS,SECMOD_TLS_FLAG),
	NSSUTIL_ARG_ENTRY(AES,SECMOD_AES_FLAG),
	NSSUTIL_ARG_ENTRY(Camellia,SECMOD_CAMELLIA_FLAG),
	NSSUTIL_ARG_ENTRY(SEED,SECMOD_SEED_FLAG),
	NSSUTIL_ARG_ENTRY(PublicCerts,SECMOD_FRIENDLY_FLAG),
	NSSUTIL_ARG_ENTRY(RANDOM,SECMOD_RANDOM_FLAG),
	NSSUTIL_ARG_ENTRY(Disable, SECMOD_DISABLE_FLAG),
};

static int nssutil_argSlotFlagTableSize = 
	sizeof(nssutil_argSlotFlagTable)/sizeof(nssutil_argSlotFlagTable[0]);


/* turn the slot flags into a bit mask */
unsigned long
NSSUTIL_ArgParseSlotFlags(char *label,char *params)
{
    char *flags,*index;
    unsigned long retValue = 0;
    int i;
    PRBool all = PR_FALSE;

    flags = NSSUTIL_ArgGetParamValue(label,params);
    if (flags == NULL) return 0;

    if (PORT_Strcasecmp(flags,"all") == 0) all = PR_TRUE;

    for (index=flags; *index; index=NSSUTIL_ArgNextFlag(index)) {
	for (i=0; i < nssutil_argSlotFlagTableSize; i++) {
	    if (all || 
		(PORT_Strncasecmp(index, nssutil_argSlotFlagTable[i].name,
				nssutil_argSlotFlagTable[i].len) == 0)) {
		retValue |= nssutil_argSlotFlagTable[i].value;
	    }
	}
    }
    PORT_Free(flags);
    return retValue;
}


/* parse a single slot specific parameter */
static void
nssutil_argDecodeSingleSlotInfo(char *name, char *params, 
                               struct NSSUTILPreSlotInfoStr *slotInfo)
{
    char *askpw;

    slotInfo->slotID=NSSUTIL_ArgDecodeNumber(name);
    slotInfo->defaultFlags=NSSUTIL_ArgParseSlotFlags("slotFlags",params);
    slotInfo->timeout=NSSUTIL_ArgReadLong("timeout",params, 0, NULL);

    askpw = NSSUTIL_ArgGetParamValue("askpw",params);
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
    slotInfo->hasRootCerts = NSSUTIL_ArgHasFlag("rootFlags", "hasRootCerts", 
                                               params);
    slotInfo->hasRootTrust = NSSUTIL_ArgHasFlag("rootFlags", "hasRootTrust", 
                                               params);
}

/* parse all the slot specific parameters. */
struct NSSUTILPreSlotInfoStr *
NSSUTIL_ArgParseSlotInfo(PLArenaPool *arena, char *slotParams, int *retCount)
{
    char *slotIndex;
    struct NSSUTILPreSlotInfoStr *slotInfo = NULL;
    int i=0,count = 0,next;

    *retCount = 0;
    if ((slotParams == NULL) || (*slotParams == 0))  return NULL;

    /* first count the number of slots */
    for (slotIndex = NSSUTIL_ArgStrip(slotParams); *slotIndex; 
	 slotIndex = NSSUTIL_ArgStrip(NSSUTIL_ArgSkipParameter(slotIndex))) {
	count++;
    }

    /* get the data structures */
    if (arena) {
	slotInfo = PORT_ArenaZNewArray(arena,
				struct NSSUTILPreSlotInfoStr, count); 
    } else {
	slotInfo = PORT_ZNewArray(struct NSSUTILPreSlotInfoStr, count);
    }
    if (slotInfo == NULL) return NULL;

    for (slotIndex = NSSUTIL_ArgStrip(slotParams), i = 0; 
					*slotIndex && i < count ; ) {
	char *name;
	name = NSSUTIL_ArgGetLabel(slotIndex,&next);
	slotIndex += next;

	if (!NSSUTIL_ArgIsBlank(*slotIndex)) {
	    char *args = NSSUTIL_ArgFetchValue(slotIndex,&next);
	    slotIndex += next;
	    if (args) {
		nssutil_argDecodeSingleSlotInfo(name,args,&slotInfo[i]);
		i++;
		PORT_Free(args);
	    }
	}
	if (name) PORT_Free(name);
	slotIndex = NSSUTIL_ArgStrip(slotIndex);
    }
    *retCount = i;
    return slotInfo;
}

/************************************************************************
 * make a new slot specific parameter 
 */
/* first make the slot flags */
static char *
nssutil_mkSlotFlags(unsigned long defaultFlags)
{
    char *flags=NULL;
    int i,j;

    for (i=0; i < sizeof(defaultFlags)*8; i++) {
	if (defaultFlags & (1UL <<i)) {
	    char *string = NULL;

	    for (j=0; j < nssutil_argSlotFlagTableSize; j++) {
		if (nssutil_argSlotFlagTable[j].value == ( 1UL << i )) {
		    string = nssutil_argSlotFlagTable[j].name;
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

/* now make the root flags */
#define NSSUTIL_MAX_ROOT_FLAG_SIZE  sizeof("hasRootCerts")+sizeof("hasRootTrust")
static char *
nssutil_mkRootFlags(PRBool hasRootCerts, PRBool hasRootTrust)
{
    char *flags= (char *)PORT_ZAlloc(NSSUTIL_MAX_ROOT_FLAG_SIZE);
    PRBool first = PR_TRUE;

    PORT_Memset(flags,0,NSSUTIL_MAX_ROOT_FLAG_SIZE);
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

/* now make a full slot string */
char *
NSSUTIL_MkSlotString(unsigned long slotID, unsigned long defaultFlags,
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
    flags = nssutil_mkSlotFlags(defaultFlags);
    rootFlags = nssutil_mkRootFlags(hasRootCerts,hasRootTrust);
    flagPair = nssutil_formatPair("slotFlags",flags,'\'');
    rootFlagsPair = nssutil_formatPair("rootFlags",rootFlags,'\'');
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
    nssutil_freePair(flagPair);
    nssutil_freePair(rootFlagsPair);
    return slotString;
}


/************************************************************************
 * Parse Full module specs into: library, commonName, module parameters,
 * and NSS specifi parameters.
 */
SECStatus
NSSUTIL_ArgParseModuleSpecEx(char *modulespec, char **lib, char **mod, 
					char **parameters, char **nss,
					char **config)
{
    int next;
    modulespec = NSSUTIL_ArgStrip(modulespec);

    *lib = *mod = *parameters = *nss = *config = 0;

    while (*modulespec) {
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*lib,"library=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*mod,"name=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*parameters,"parameters=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*nss,"nss=",;)
        NSSUTIL_HANDLE_STRING_ARG(modulespec,*config,"config=",;)
	NSSUTIL_HANDLE_FINAL_ARG(modulespec)
   }
   return SECSuccess;
}

/************************************************************************
 * Parse Full module specs into: library, commonName, module parameters,
 * and NSS specifi parameters.
 */
SECStatus
NSSUTIL_ArgParseModuleSpec(char *modulespec, char **lib, char **mod, 
					char **parameters, char **nss)
{
    int next;
    modulespec = NSSUTIL_ArgStrip(modulespec);

    *lib = *mod = *parameters = *nss = 0;

    while (*modulespec) {
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*lib,"library=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*mod,"name=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*parameters,"parameters=",;)
	NSSUTIL_HANDLE_STRING_ARG(modulespec,*nss,"nss=",;)
	NSSUTIL_HANDLE_FINAL_ARG(modulespec)
   }
   return SECSuccess;
}

/************************************************************************
 * make a new module spec from it's components */
char *
NSSUTIL_MkModuleSpecEx(char *dllName, char *commonName, char *parameters, 
								char *NSS,
								char *config)
{
    char *moduleSpec;
    char *lib,*name,*param,*nss,*conf;

    /*
     * now the final spec
     */
    lib = nssutil_formatPair("library",dllName,'\"');
    name = nssutil_formatPair("name",commonName,'\"');
    param = nssutil_formatPair("parameters",parameters,'\"');
    nss = nssutil_formatPair("NSS",NSS,'\"');
    if (config) {
        conf = nssutil_formatPair("config",config,'\"');
        moduleSpec = PR_smprintf("%s %s %s %s %s", lib,name,param,nss,conf);
        nssutil_freePair(conf);
    } else {
        moduleSpec = PR_smprintf("%s %s %s %s", lib,name,param,nss);
    }
    nssutil_freePair(lib);
    nssutil_freePair(name);
    nssutil_freePair(param);
    nssutil_freePair(nss);
    return (moduleSpec);
}

/************************************************************************
 * make a new module spec from it's components */
char *
NSSUTIL_MkModuleSpec(char *dllName, char *commonName, char *parameters, 
								char *NSS)
{
    return NSSUTIL_MkModuleSpecEx(dllName, commonName, parameters, NSS, NULL);
}


#define NSSUTIL_ARG_FORTEZZA_FLAG "FORTEZZA"
/******************************************************************************
 * Parse the cipher flags from the NSS parameter
 */
void
NSSUTIL_ArgParseCipherFlags(unsigned long *newCiphers,char *cipherList)
{
    newCiphers[0] = newCiphers[1] = 0;
    if ((cipherList == NULL) || (*cipherList == 0)) return;

    for (;*cipherList; cipherList=NSSUTIL_ArgNextFlag(cipherList)) {
	if (PORT_Strncasecmp(cipherList,NSSUTIL_ARG_FORTEZZA_FLAG,
				sizeof(NSSUTIL_ARG_FORTEZZA_FLAG)-1) == 0) {
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


/*********************************************************************
 * make NSS parameter...
 */
/* First make NSS specific flags */
#define MAX_FLAG_SIZE  sizeof("internal")+sizeof("FIPS")+sizeof("moduleDB")+\
				sizeof("moduleDBOnly")+sizeof("critical")
static char *
nssutil_mkNSSFlags(PRBool internal, PRBool isFIPS,
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


/* construct the NSS cipher flags */
static char *
nssutil_mkCipherFlags(unsigned long ssl0, unsigned long ssl1)
{
    char *cipher = NULL;
    int i;

    for (i=0; i < sizeof(ssl0)*8; i++) {
	if (ssl0 & (1UL <<i)) {
	    char *string;
	    if ((1UL <<i) == SECMOD_FORTEZZA_FLAG) {
		string = PR_smprintf("%s",NSSUTIL_ARG_FORTEZZA_FLAG);
	    } else {
		string = PR_smprintf("0h0x%08lx", 1UL <<i);
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
	if (ssl1 & (1UL <<i)) {
	    if (cipher) {
		char *tmp;
		tmp = PR_smprintf("%s,0l0x%08lx",cipher, 1UL <<i);
		PR_smprintf_free(cipher);
		cipher = tmp;
	    } else {
		cipher = PR_smprintf("0l0x%08lx", 1UL <<i);
	    }
	}
    }

    return cipher;
}

/* Assemble a full NSS string. */
char *
NSSUTIL_MkNSSString(char **slotStrings, int slotCount, PRBool internal, 
	  PRBool isFIPS, PRBool isModuleDB,  PRBool isModuleDBOnly, 
	  PRBool isCritical, unsigned long trustOrder, 
	  unsigned long cipherOrder, unsigned long ssl0, unsigned long ssl1)
{
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
    nssFlags = nssutil_mkNSSFlags(internal,isFIPS,isModuleDB,isModuleDBOnly,
							isCritical); 
	/* for now only the internal module is critical */
    ciphers = nssutil_mkCipherFlags(ssl0, ssl1);

    trustOrderPair = nssutil_formatIntPair("trustOrder",trustOrder,
					NSSUTIL_DEFAULT_TRUST_ORDER);
    cipherOrderPair = nssutil_formatIntPair("cipherOrder",cipherOrder,
					NSSUTIL_DEFAULT_CIPHER_ORDER);
    slotPair=nssutil_formatPair("slotParams",slotParams,'{'); /* } */
    if (slotParams) PORT_Free(slotParams);
    cipherPair=nssutil_formatPair("ciphers",ciphers,'\'');
    if (ciphers) PR_smprintf_free(ciphers);
    flagPair=nssutil_formatPair("Flags",nssFlags,'\'');
    if (nssFlags) PORT_Free(nssFlags);
    nss = PR_smprintf("%s %s %s %s %s",trustOrderPair,
			cipherOrderPair,slotPair,cipherPair,flagPair);
    nssutil_freePair(trustOrderPair);
    nssutil_freePair(cipherOrderPair);
    nssutil_freePair(slotPair);
    nssutil_freePair(cipherPair);
    nssutil_freePair(flagPair);
    tmp = NSSUTIL_ArgStrip(nss);
    if (*tmp == '\0') {
	PR_smprintf_free(nss);
	nss = NULL;
    }
    return nss;
}

/*****************************************************************************
 *
 * Private calls for use by softoken and utilmod.c
 */

#define SQLDB "sql:"
#define EXTERNDB "extern:"
#define LEGACY "dbm:"
#define MULTIACCESS "multiaccess:"
#define SECMOD_DB "secmod.db"
const char *
_NSSUTIL_EvaluateConfigDir(const char *configdir, 
			   NSSDBType *pdbType, char **appName)
{
    NSSDBType dbType;
    *appName = NULL;
/* force the default */
#ifdef NSS_DISABLE_DBM
    dbType = NSS_DB_TYPE_SQL;
#else
    dbType = NSS_DB_TYPE_LEGACY;
#endif
    if (PORT_Strncmp(configdir, MULTIACCESS, sizeof(MULTIACCESS)-1) == 0) {
	char *cdir;
	dbType = NSS_DB_TYPE_MULTIACCESS;

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
	dbType = NSS_DB_TYPE_SQL;
	configdir = configdir + sizeof(SQLDB) -1;
    } else if (PORT_Strncmp(configdir, EXTERNDB, sizeof(EXTERNDB)-1) == 0) {
	dbType = NSS_DB_TYPE_EXTERN;
	configdir = configdir + sizeof(EXTERNDB) -1;
    } else if (PORT_Strncmp(configdir, LEGACY, sizeof(LEGACY)-1) == 0) {
	dbType = NSS_DB_TYPE_LEGACY;
	configdir = configdir + sizeof(LEGACY) -1;
    } else {
	/* look up the default from the environment */
	char *defaultType = PR_GetEnv("NSS_DEFAULT_DB_TYPE");
	if (defaultType != NULL) {
	    if (PORT_Strncmp(defaultType, SQLDB, sizeof(SQLDB)-2) == 0) {
		dbType = NSS_DB_TYPE_SQL;
	    } else if (PORT_Strncmp(defaultType,EXTERNDB,sizeof(EXTERNDB)-2)==0) {
		dbType = NSS_DB_TYPE_EXTERN;
	    } else if (PORT_Strncmp(defaultType, LEGACY, sizeof(LEGACY)-2) == 0) {
		dbType = NSS_DB_TYPE_LEGACY;
	    }
	}
    }
    /* if the caller has already set a type, don't change it */
    if (*pdbType == NSS_DB_TYPE_NONE) {
	*pdbType = dbType;
    }
    return configdir;
}

char *
_NSSUTIL_GetSecmodName(char *param, NSSDBType *dbType, char **appName,
		   char **filename, PRBool *rw)
{
    int next;
    char *configdir = NULL;
    char *secmodName = NULL;
    char *value = NULL;
    char *save_params = param;
    const char *lconfigdir;
    PRBool noModDB = PR_FALSE;
    param = NSSUTIL_ArgStrip(param);
	

    while (*param) {
	NSSUTIL_HANDLE_STRING_ARG(param,configdir,"configDir=",;)
	NSSUTIL_HANDLE_STRING_ARG(param,secmodName,"secmod=",;)
	NSSUTIL_HANDLE_FINAL_ARG(param)
   }

   *rw = PR_TRUE;
   if (NSSUTIL_ArgHasFlag("flags","readOnly",save_params)) {
	*rw = PR_FALSE;
   }

   if (!secmodName || *secmodName == '\0') {
	if (secmodName) PORT_Free(secmodName);
	secmodName = PORT_Strdup(SECMOD_DB);
   }

   *filename = secmodName;
   lconfigdir = _NSSUTIL_EvaluateConfigDir(configdir, dbType, appName);

   if (NSSUTIL_ArgHasFlag("flags","noModDB",save_params)) {
	/* there isn't a module db, don't load the legacy support */
	noModDB = PR_TRUE;
	*dbType = NSS_DB_TYPE_SQL;
	PORT_Free(*filename);
	*filename = NULL;
        *rw = PR_FALSE;
   }

   /* only use the renamed secmod for legacy databases */
   if ((*dbType != NSS_DB_TYPE_LEGACY) && 
	(*dbType != NSS_DB_TYPE_MULTIACCESS)) {
	secmodName="pkcs11.txt";
   }

   if (noModDB) {
	value = NULL;
   } else if (lconfigdir && lconfigdir[0] != '\0') {
	value = PR_smprintf("%s" NSSUTIL_PATH_SEPARATOR "%s",
			lconfigdir,secmodName);
   } else {
	value = PR_smprintf("%s",secmodName);
   }
   if (configdir) PORT_Free(configdir);
   return value;
}


