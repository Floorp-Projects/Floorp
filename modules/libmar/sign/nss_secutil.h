/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* With the exception of GetPasswordString, this file was
   copied from NSS's cmd/lib/secutil.h hg revision 8f011395145e */

#ifndef NSS_SECUTIL_H_
#define NSS_SECUTIL_H_

#include "nss.h"
#include "pk11pub.h"
#include "cryptohi.h"
#include "hasht.h"
#include "cert.h"
#include "key.h"
#include <stdint.h>

typedef struct {
  enum {
    PW_NONE = 0,
    PW_FROMFILE = 1,
    PW_PLAINTEXT = 2,
    PW_EXTERNAL = 3
  } source;
  char *data;
} secuPWData;

#if( defined(_WINDOWS) && !defined(_WIN32_WCE))
#include <conio.h>
#include <io.h>
#define QUIET_FGETS quiet_fgets
static char * quiet_fgets (char *buf, int length, FILE *input);
#else
#define QUIET_FGETS fgets
#endif

char *
SECU_GetModulePassword(PK11SlotInfo *slot, PRBool retry, void *arg);

#endif
