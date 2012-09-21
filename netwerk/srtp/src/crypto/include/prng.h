/*
 * prng.h
 *
 * pseudorandom source
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */

#ifndef PRNG_H
#define PRNG_H

#include "rand_source.h"  /* for rand_source_func_t definition       */
#include "aes.h"          /* for aes                                 */
#include "aes_icm.h"      /* for aes ctr                             */

#define MAX_PRNG_OUT_LEN 0xffffffffU

/*
 * x917_prng is an ANSI X9.17-like AES-based PRNG
 */

typedef struct {
  v128_t   state;          /* state data                              */
  aes_expanded_key_t key;  /* secret key                              */
  uint32_t octet_count;    /* number of octets output since last init */
  rand_source_func_t rand; /* random source for re-initialization     */
} x917_prng_t;

err_status_t
x917_prng_init(rand_source_func_t random_source);

err_status_t
x917_prng_get_octet_string(uint8_t *dest, uint32_t len);


/*
 * ctr_prng is an AES-CTR based PRNG
 */

typedef struct {
  uint32_t octet_count;    /* number of octets output since last init */
  aes_icm_ctx_t   state;   /* state data                              */
  rand_source_func_t rand; /* random source for re-initialization     */
} ctr_prng_t;

err_status_t
ctr_prng_init(rand_source_func_t random_source);

err_status_t
ctr_prng_get_octet_string(void *dest, uint32_t len);


#endif
