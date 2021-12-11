/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "pkcs11i.h"
#include "sftkdbt.h"

/* parsing functions */
char *sftk_argFetchValue(char *string, int *pcount);
char *sftk_getSecmodName(char *param, SDBType *dbType, char **appName, char **filename, PRBool *rw);
char *sftk_argStrip(char *c);
CK_RV sftk_parseParameters(char *param, sftk_parameters *parsed, PRBool isFIPS);
void sftk_freeParams(sftk_parameters *params);
const char *sftk_EvaluateConfigDir(const char *configdir, SDBType *dbType, char **app);
char *sftk_argGetParamValue(char *paramName, char *parameters);
