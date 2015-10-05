/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _UTILPARS_H_
#define _UTILPARS_H_ 1

#include "utilparst.h"
#include "plarena.h"

/* handle a module db request */
char ** NSSUTIL_DoModuleDBFunction(unsigned long function,char *parameters, void *args);

/* parsing functions */
char *NSSUTIL_ArgFetchValue(char *string, int *pcount);
char *NSSUTIL_ArgStrip(char *c);
char *NSSUTIL_ArgGetParamValue(char *paramName,char *parameters);
char *NSSUTIL_ArgSkipParameter(char *string);
char *NSSUTIL_ArgGetLabel(char *inString, int *next);
long NSSUTIL_ArgDecodeNumber(char *num);
PRBool NSSUTIL_ArgIsBlank(char c);
PRBool NSSUTIL_ArgHasFlag(char *label, char *flag, char *parameters);
long NSSUTIL_ArgReadLong(char *label,char *params, long defValue, 
			 PRBool *isdefault);

/* quoting functions */
int NSSUTIL_EscapeSize(const char *string, char quote);
char *NSSUTIL_Escape(const char *string, char quote);
int NSSUTIL_QuoteSize(const char *string, char quote);
char *NSSUTIL_Quote(const char *string, char quote);
int NSSUTIL_DoubleEscapeSize(const char *string, char quote1, char quote2);
char *NSSUTIL_DoubleEscape(const char *string, char quote1, char quote2);

unsigned long NSSUTIL_ArgParseSlotFlags(char *label,char *params);
struct NSSUTILPreSlotInfoStr *NSSUTIL_ArgParseSlotInfo(PLArenaPool *arena,
					 char *slotParams, int *retCount);
char * NSSUTIL_MkSlotString(unsigned long slotID, unsigned long defaultFlags,
                  unsigned long timeout, unsigned char askpw_in,
                  PRBool hasRootCerts, PRBool hasRootTrust);
SECStatus NSSUTIL_ArgParseModuleSpec(char *modulespec, char **lib, char **mod,
                                        char **parameters, char **nss);
char *NSSUTIL_MkModuleSpec(char *dllName, char *commonName, 
					char *parameters, char *NSS);
void NSSUTIL_ArgParseCipherFlags(unsigned long *newCiphers,char *cipherList);
char * NSSUTIL_MkNSSString(char **slotStrings, int slotCount, PRBool internal,
          PRBool isFIPS, PRBool isModuleDB,  PRBool isModuleDBOnly,
          PRBool isCritical, unsigned long trustOrder,
          unsigned long cipherOrder, unsigned long ssl0, unsigned long ssl1);

/*
 * private functions for softoken.
 */
char * _NSSUTIL_GetSecmodName(char *param, NSSDBType *dbType, char **appName, char **filename,PRBool *rw);
const char *_NSSUTIL_EvaluateConfigDir(const char *configdir, NSSDBType *dbType, char **app);

#endif /* _UTILPARS_H_ */
