/*
 * stat-driver.c
 *
 * test driver for the stat_test functions
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>         /* for printf() */

#include "err.h"
#include "stat.h"

#include "cipher.h"

typedef struct {
  void *state;
} random_source_t;

err_status_t
random_source_alloc(void);

void
err_check(err_status_t s) {
  if (s) {
    printf("error (code %d)\n", s);
    exit(1);
  }
}

int
main (int argc, char *argv[]) {
  uint8_t buffer[2500];
  unsigned int buf_len = 2500;
  int i, j;
  extern cipher_type_t aes_icm;
  cipher_t *c;
  uint8_t key[46] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05
    };
  v128_t nonce;
  int num_trials = 500;
  int num_fail;

  printf("statistical tests driver\n");

  for (i=0; i < 2500; i++)
    buffer[i] = 0;

  /* run tests */
  printf("running stat_tests on all-null buffer, expecting failure\n");
  printf("monobit %d\n", stat_test_monobit(buffer));
  printf("poker   %d\n", stat_test_poker(buffer));
  printf("runs    %d\n", stat_test_runs(buffer));

  for (i=0; i < 2500; i++)
    buffer[i] = rand();
  printf("running stat_tests on rand(), expecting success\n");
  printf("monobit %d\n", stat_test_monobit(buffer));
  printf("poker   %d\n", stat_test_poker(buffer));
  printf("runs    %d\n", stat_test_runs(buffer));

  printf("running stat_tests on AES-128-ICM, expecting success\n");
  /* set buffer to cipher output */
  for (i=0; i < 2500; i++)
    buffer[i] = 0;
  err_check(cipher_type_alloc(&aes_icm, &c, 30));
  err_check(cipher_init(c, key, direction_encrypt));
  err_check(cipher_set_iv(c, &nonce));
  err_check(cipher_encrypt(c, buffer, &buf_len));
  /* run tests on cipher outout */
  printf("monobit %d\n", stat_test_monobit(buffer));
  printf("poker   %d\n", stat_test_poker(buffer));
  printf("runs    %d\n", stat_test_runs(buffer));

  printf("runs test (please be patient): ");
  fflush(stdout);
  num_fail = 0;
  v128_set_to_zero(&nonce);
  for(j=0; j < num_trials; j++) {
    for (i=0; i < 2500; i++)
      buffer[i] = 0;
    nonce.v32[3] = i;
    err_check(cipher_set_iv(c, &nonce));
    err_check(cipher_encrypt(c, buffer, &buf_len));
    if (stat_test_runs(buffer)) {
      num_fail++;
    }
  }

  printf("%d failures in %d tests\n", num_fail, num_trials);
  printf("(nota bene: a small fraction of stat_test failures does not \n"
	 "indicate that the random source is invalid)\n");

  err_check(cipher_dealloc(c));

  printf("running stat_tests on AES-256-ICM, expecting success\n");
  /* set buffer to cipher output */
  for (i=0; i < 2500; i++)
    buffer[i] = 0;
  err_check(cipher_type_alloc(&aes_icm, &c, 46));
  err_check(cipher_init(c, key, direction_encrypt));
  err_check(cipher_set_iv(c, &nonce));
  err_check(cipher_encrypt(c, buffer, &buf_len));
  /* run tests on cipher outout */
  printf("monobit %d\n", stat_test_monobit(buffer));
  printf("poker   %d\n", stat_test_poker(buffer));
  printf("runs    %d\n", stat_test_runs(buffer));

  printf("runs test (please be patient): ");
  fflush(stdout);
  num_fail = 0;
  v128_set_to_zero(&nonce);
  for(j=0; j < num_trials; j++) {
    for (i=0; i < 2500; i++)
      buffer[i] = 0;
    nonce.v32[3] = i;
    err_check(cipher_set_iv(c, &nonce));
    err_check(cipher_encrypt(c, buffer, &buf_len));
    if (stat_test_runs(buffer)) {
      num_fail++;
    }
  }

  printf("%d failures in %d tests\n", num_fail, num_trials);
  printf("(nota bene: a small fraction of stat_test failures does not \n"
	 "indicate that the random source is invalid)\n");

  err_check(cipher_dealloc(c));

  return 0;
}
