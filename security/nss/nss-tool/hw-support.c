/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

/* This is a freebl command line utility that prints hardware support as freebl
 * sees it from its detection in blinit.c
 */

#include <stdio.h>

#include "blapi.h"
#include "blapii.h"
#include "nss.h"

int main(int argc, char const *argv[]) {
  BL_Init();
  printf("\n\n ========== NSS Hardware Report ==========\n");
#if defined(NSS_X86_OR_X64)
  printf("\tAES-NI \t%s supported\n", aesni_support() ? "" : "not");
  printf("\tPCLMUL \t%s supported\n", clmul_support() ? "" : "not");
  printf("\tAVX \t%s supported\n", avx_support() ? "" : "not");
  printf("\tSSSE3 \t%s supported\n", ssse3_support() ? "" : "not");
  printf("\tSSE4.1 \t%s supported\n", sse4_1_support() ? "" : "not");
  printf("\tSSE4.2 \t%s supported\n", sse4_2_support() ? "" : "not");
#elif defined(__aarch64__) || defined(__arm__)
  printf("\tNEON \t%s supported\n", arm_neon_support() ? "" : "not");
  printf("\tAES \t%s supported\n", arm_aes_support() ? "" : "not");
  printf("\tPMULL \t%s supported\n", arm_pmull_support() ? "" : "not");
  printf("\tSHA1 \t%s supported\n", arm_sha1_support() ? "" : "not");
  printf("\tSHA2 \t%s supported\n", arm_sha2_support() ? "" : "not");
#endif
  printf(" ========== Hardware Report End ==========\n\n\n");
  BL_Cleanup();
  return 0;
}
