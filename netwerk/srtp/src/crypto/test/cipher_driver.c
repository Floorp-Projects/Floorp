/*
 * cipher_driver.c
 *
 * A driver for the generic cipher type
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

#include <stdio.h>           /* for printf() */
#include <stdlib.h>          /* for rand() */
#include <string.h>          /* for memset() */
#include <unistd.h>          /* for getopt() */
#include "cipher.h"
#include "aes_icm.h"
#include "null_cipher.h"

#define PRINT_DEBUG 0

void
cipher_driver_test_throughput(cipher_t *c);

err_status_t
cipher_driver_self_test(cipher_type_t *ct);


/*
 * cipher_driver_test_buffering(ct) tests the cipher's output
 * buffering for correctness by checking the consistency of succesive
 * calls
 */

err_status_t
cipher_driver_test_buffering(cipher_t *c);


/*
 * functions for testing cipher cache thrash
 */
err_status_t
cipher_driver_test_array_throughput(cipher_type_t *ct, 
				    int klen, int num_cipher);

void
cipher_array_test_throughput(cipher_t *ca[], int num_cipher);

uint64_t
cipher_array_bits_per_second(cipher_t *cipher_array[], int num_cipher, 
			     unsigned octets_in_buffer, int num_trials);

err_status_t
cipher_array_delete(cipher_t *cipher_array[], int num_cipher);

err_status_t
cipher_array_alloc_init(cipher_t ***cipher_array, int num_ciphers,
			cipher_type_t *ctype, int klen);

void
usage(char *prog_name) {
  printf("usage: %s [ -t | -v | -a ]\n", prog_name);
  exit(255);
}

void
check_status(err_status_t s) {
  if (s) {
    printf("error (code %d)\n", s);
    exit(s);
  }
  return;
}

/*
 * null_cipher, aes_icm, and aes_cbc are the cipher meta-objects
 * defined in the files in crypto/cipher subdirectory.  these are
 * declared external so that we can use these cipher types here
 */

extern cipher_type_t null_cipher;
extern cipher_type_t aes_icm;
extern cipher_type_t aes_cbc;

int
main(int argc, char *argv[]) {
  cipher_t *c = NULL;
  err_status_t status;
  unsigned char test_key[48] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  };
  int q;
  unsigned do_timing_test = 0;
  unsigned do_validation = 0;
  unsigned do_array_timing_test = 0;

  /* process input arguments */
  while (1) {
    q = getopt(argc, argv, "tva");
    if (q == -1) 
      break;
    switch (q) {
    case 't':
      do_timing_test = 1;
      break;
    case 'v':
      do_validation = 1;
      break;
    case 'a':
      do_array_timing_test = 1;
      break;
    default:
      usage(argv[0]);
    }    
  }
   
  printf("cipher test driver\n"
	 "David A. McGrew\n"
	 "Cisco Systems, Inc.\n");

  if (!do_validation && !do_timing_test && !do_array_timing_test)
    usage(argv[0]);

   /* arry timing (cache thrash) test */
  if (do_array_timing_test) {
    int max_num_cipher = 1 << 16;   /* number of ciphers in cipher_array */
    int num_cipher;
    
    for (num_cipher=1; num_cipher < max_num_cipher; num_cipher *=8)
      cipher_driver_test_array_throughput(&null_cipher, 0, num_cipher); 

    for (num_cipher=1; num_cipher < max_num_cipher; num_cipher *=8)
      cipher_driver_test_array_throughput(&aes_icm, 30, num_cipher); 

    for (num_cipher=1; num_cipher < max_num_cipher; num_cipher *=8)
      cipher_driver_test_array_throughput(&aes_icm, 46, num_cipher); 

    for (num_cipher=1; num_cipher < max_num_cipher; num_cipher *=8)
      cipher_driver_test_array_throughput(&aes_cbc, 16, num_cipher); 
 
    for (num_cipher=1; num_cipher < max_num_cipher; num_cipher *=8)
      cipher_driver_test_array_throughput(&aes_cbc, 32, num_cipher); 
  }

  if (do_validation) {
    cipher_driver_self_test(&null_cipher);
    cipher_driver_self_test(&aes_icm);
    cipher_driver_self_test(&aes_cbc);
  }

  /* do timing and/or buffer_test on null_cipher */
  status = cipher_type_alloc(&null_cipher, &c, 0); 
  check_status(status);

  status = cipher_init(c, NULL, direction_encrypt);
  check_status(status);

  if (do_timing_test) 
    cipher_driver_test_throughput(c);
  if (do_validation) {
    status = cipher_driver_test_buffering(c);
    check_status(status);
  }
  status = cipher_dealloc(c);
  check_status(status);
  

  /* run the throughput test on the aes_icm cipher (128-bit key) */
    status = cipher_type_alloc(&aes_icm, &c, 30);  
    if (status) {
      fprintf(stderr, "error: can't allocate cipher\n");
      exit(status);
    }

    status = cipher_init(c, test_key, direction_encrypt);
    check_status(status);

    if (do_timing_test)
      cipher_driver_test_throughput(c);
    
    if (do_validation) {
      status = cipher_driver_test_buffering(c);
      check_status(status);
    }
    
    status = cipher_dealloc(c);
    check_status(status);

  /* repeat the tests with 256-bit keys */
    status = cipher_type_alloc(&aes_icm, &c, 46);  
    if (status) {
      fprintf(stderr, "error: can't allocate cipher\n");
      exit(status);
    }

    status = cipher_init(c, test_key, direction_encrypt);
    check_status(status);

    if (do_timing_test)
      cipher_driver_test_throughput(c);
    
    if (do_validation) {
      status = cipher_driver_test_buffering(c);
      check_status(status);
    }
    
    status = cipher_dealloc(c);
    check_status(status);
  
  return 0;
}

void
cipher_driver_test_throughput(cipher_t *c) {
  int i;
  int min_enc_len = 32;     
  int max_enc_len = 2048;   /* should be a power of two */
  int num_trials = 1000000;  
  
  printf("timing %s throughput, key length %d:\n", c->type->description, c->key_len);
  fflush(stdout);
  for (i=min_enc_len; i <= max_enc_len; i = i * 2)
    printf("msg len: %d\tgigabits per second: %f\n",
	   i, cipher_bits_per_second(c, i, num_trials) / 1e9);

}

err_status_t
cipher_driver_self_test(cipher_type_t *ct) {
  err_status_t status;
  
  printf("running cipher self-test for %s...", ct->description);
  status = cipher_type_self_test(ct);
  if (status) {
    printf("failed with error code %d\n", status);
    exit(status);
  }
  printf("passed\n");
  
  return err_status_ok;
}

/*
 * cipher_driver_test_buffering(ct) tests the cipher's output
 * buffering for correctness by checking the consistency of succesive
 * calls
 */

err_status_t
cipher_driver_test_buffering(cipher_t *c) {
  int i, j, num_trials = 1000;
  unsigned len, buflen = 1024;
  uint8_t buffer0[buflen], buffer1[buflen], *current, *end;
  uint8_t idx[16] = { 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34
  };
  err_status_t status;
  
  printf("testing output buffering for cipher %s...",
	 c->type->description);

  for (i=0; i < num_trials; i++) {

   /* set buffers to zero */
    for (j=0; j < buflen; j++) 
      buffer0[j] = buffer1[j] = 0;
    
    /* initialize cipher  */
    status = cipher_set_iv(c, idx);
    if (status)
      return status;

    /* generate 'reference' value by encrypting all at once */
    status = cipher_encrypt(c, buffer0, &buflen);
    if (status)
      return status;

    /* re-initialize cipher */
    status = cipher_set_iv(c, idx);
    if (status)
      return status;
    
    /* now loop over short lengths until buffer1 is encrypted */
    current = buffer1;
    end = buffer1 + buflen;
    while (current < end) {

      /* choose a short length */
      len = rand() & 0x01f;

      /* make sure that len doesn't cause us to overreach the buffer */
      if (current + len > end)
	len = end - current;

      status = cipher_encrypt(c, current, &len);
      if (status) 
	return status;
      
      /* advance pointer into buffer1 to reflect encryption */
      current += len;
      
      /* if buffer1 is all encrypted, break out of loop */
      if (current == end)
	break;
    }

    /* compare buffers */
    for (j=0; j < buflen; j++)
      if (buffer0[j] != buffer1[j]) {
#if PRINT_DEBUG
	printf("test case %d failed at byte %d\n", i, j);
	printf("computed: %s\n", octet_string_hex_string(buffer1, buflen));
	printf("expected: %s\n", octet_string_hex_string(buffer0, buflen));
#endif 
	return err_status_algo_fail;
      }
  }
  
  printf("passed\n");

  return err_status_ok;
}


/*
 * The function cipher_test_throughput_array() tests the effect of CPU
 * cache thrash on cipher throughput.  
 *
 * cipher_array_alloc_init(ctype, array, num_ciphers) creates an array
 * of cipher_t of type ctype
 */

err_status_t
cipher_array_alloc_init(cipher_t ***ca, int num_ciphers,
			cipher_type_t *ctype, int klen) {
  int i, j;
  err_status_t status;
  uint8_t *key;
  cipher_t **cipher_array;
  /* pad klen allocation, to handle aes_icm reading 16 bytes for the
     14-byte salt */
  int klen_pad = ((klen + 15) >> 4) << 4;

  /* allocate array of pointers to ciphers */
  cipher_array = (cipher_t **) malloc(sizeof(cipher_t *) * num_ciphers);
  if (cipher_array == NULL)
    return err_status_alloc_fail;

  /* set ca to location of cipher_array */
  *ca = cipher_array;

  /* allocate key */
  key = crypto_alloc(klen_pad);
  if (key == NULL) {
    free(cipher_array);
    return err_status_alloc_fail;
  }
  
  /* allocate and initialize an array of ciphers */
  for (i=0; i < num_ciphers; i++) {

    /* allocate cipher */
    status = cipher_type_alloc(ctype, cipher_array, klen);
    if (status)
      return status;
    
    /* generate random key and initialize cipher */
    for (j=0; j < klen; j++)
      key[j] = (uint8_t) rand();
    for (; j < klen_pad; j++)
      key[j] = 0;
    status = cipher_init(*cipher_array, key, direction_encrypt);
    if (status)
      return status;

/*     printf("%dth cipher is at %p\n", i, *cipher_array); */
/*     printf("%dth cipher description: %s\n", i,  */
/* 	   (*cipher_array)->type->description); */
    
    /* advance cipher array pointer */
    cipher_array++;
  }

  crypto_free(key);

  return err_status_ok;
}

err_status_t
cipher_array_delete(cipher_t *cipher_array[], int num_cipher) {
  int i;
  
  for (i=0; i < num_cipher; i++) {
    cipher_dealloc(cipher_array[i]);
  }

  free(cipher_array);
  
  return err_status_ok;
}


/*
 * cipher_array_bits_per_second(c, l, t) computes (an estimate of) the
 * number of bits that a cipher implementation can encrypt in a second
 * when distinct keys are used to encrypt distinct messages
 * 
 * c is a cipher (which MUST be allocated an initialized already), l
 * is the length in octets of the test data to be encrypted, and t is
 * the number of trials
 *
 * if an error is encountered, the value 0 is returned
 */

uint64_t
cipher_array_bits_per_second(cipher_t *cipher_array[], int num_cipher, 
			      unsigned octets_in_buffer, int num_trials) {
  int i;
  v128_t nonce;
  clock_t timer;
  unsigned char *enc_buf;
  int cipher_index = rand() % num_cipher;

  /* Over-alloc, for NIST CBC padding */
  enc_buf = crypto_alloc(octets_in_buffer+17);
  if (enc_buf == NULL)
    return 0;  /* indicate bad parameters by returning null */
  memset(enc_buf, 0, octets_in_buffer);
  
  /* time repeated trials */
  v128_set_to_zero(&nonce);
  timer = clock();
  for(i=0; i < num_trials; i++, nonce.v32[3] = i) {
    /* length parameter to cipher_encrypt is in/out -- out is total, padded
     * length -- so reset it each time. */
    unsigned octets_to_encrypt = octets_in_buffer;

    /* encrypt buffer with cipher */
    cipher_set_iv(cipher_array[cipher_index], &nonce);
    cipher_encrypt(cipher_array[cipher_index], enc_buf, &octets_to_encrypt);

    /* choose a cipher at random from the array*/
    cipher_index = (*((uint32_t *)enc_buf)) % num_cipher;
  }
  timer = clock() - timer;

  free(enc_buf);

  if (timer == 0) {
    /* Too fast! */
    return 0;
  }

  return (uint64_t)CLOCKS_PER_SEC * num_trials * 8 * octets_in_buffer / timer;
}

void
cipher_array_test_throughput(cipher_t *ca[], int num_cipher) {
  int i;
  int min_enc_len = 16;     
  int max_enc_len = 2048;   /* should be a power of two */
  int num_trials = 1000000;

  printf("timing %s throughput with key length %d, array size %d:\n", 
	 (ca[0])->type->description, (ca[0])->key_len, num_cipher);
  fflush(stdout);
  for (i=min_enc_len; i <= max_enc_len; i = i * 4)
    printf("msg len: %d\tgigabits per second: %f\n", i,
	   cipher_array_bits_per_second(ca, num_cipher, i, num_trials) / 1e9);

}

err_status_t
cipher_driver_test_array_throughput(cipher_type_t *ct, 
				    int klen, int num_cipher) {
  cipher_t **ca = NULL;
  err_status_t status;

  status = cipher_array_alloc_init(&ca, num_cipher, ct, klen);
  if (status) {
    printf("error: cipher_array_alloc_init() failed with error code %d\n",
	   status);
    return status;
  }
  
  cipher_array_test_throughput(ca, num_cipher);
  
  cipher_array_delete(ca, num_cipher);    
 
  return err_status_ok;
}
