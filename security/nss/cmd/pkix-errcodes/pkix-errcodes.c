/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * dumpcert.c
 *
 * dump certificate sample application
 *
 */

#include <stdio.h>

#include "pkix.h"
#include "pkixt.h"
#include "pkix_error.h"

#undef PKIX_ERRORENTRY
#define PKIX_ERRORENTRY(name,desc,plerr) #name

const char * const PKIX_ErrorNames[] =
{
#include "pkix_errorstrings.h"
};

#undef PKIX_ERRORENTRY


int
main(int argc, char **argv)
{
  int i = 0;
  for (; i < PKIX_NUMERRORCODES; ++i) {
    printf("code %d %s\n", i, PKIX_ErrorNames[i]);
  }
  return 0;
}
