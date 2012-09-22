/*
 * crypto_kernel.c
 *
 * header for the cryptographic kernel
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright(c) 2001-2006 Cisco Systems, Inc.
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


#include "alloc.h"

#include "crypto_kernel.h"

/* the debug module for the crypto_kernel */

debug_module_t mod_crypto_kernel = {
  0,                  /* debugging is off by default */
  "crypto kernel"     /* printable name for module   */
};

/*
 * other debug modules that can be included in the kernel
 */

extern debug_module_t mod_auth;
extern debug_module_t mod_cipher;
extern debug_module_t mod_stat;
extern debug_module_t mod_alloc;

/* 
 * cipher types that can be included in the kernel
 */ 

extern cipher_type_t null_cipher;
extern cipher_type_t aes_icm;
extern cipher_type_t aes_cbc;


/*
 * auth func types that can be included in the kernel
 */

extern auth_type_t null_auth;
extern auth_type_t hmac;

/* crypto_kernel is a global variable, the only one of its datatype */

crypto_kernel_t
crypto_kernel = {
  crypto_kernel_state_insecure,    /* start off in insecure state */
  NULL,                            /* no cipher types yet         */
  NULL,                            /* no auth types yet           */
  NULL                             /* no debug modules yet        */
};

#define MAX_RNG_TRIALS 25

err_status_t
crypto_kernel_init() {
  err_status_t status;  

  /* check the security state */
  if (crypto_kernel.state == crypto_kernel_state_secure) {
    
    /*
     * we're already in the secure state, but we've been asked to
     * re-initialize, so we just re-run the self-tests and then return
     */
    return crypto_kernel_status(); 
  }

  /* initialize error reporting system */
  status = err_reporting_init("crypto");
  if (status)
    return status;

  /* load debug modules */
  status = crypto_kernel_load_debug_module(&mod_crypto_kernel);
  if (status)
    return status;
  status = crypto_kernel_load_debug_module(&mod_auth);
  if (status)
    return status;
  status = crypto_kernel_load_debug_module(&mod_cipher);
  if (status)
    return status;
  status = crypto_kernel_load_debug_module(&mod_stat);
  if (status)
    return status;
  status = crypto_kernel_load_debug_module(&mod_alloc);
  if (status)
    return status;
  
  /* initialize random number generator */
  status = rand_source_init();
  if (status)
    return status;

  /* run FIPS-140 statistical tests on rand_source */  
  status = stat_test_rand_source_with_repetition(rand_source_get_octet_string, MAX_RNG_TRIALS);
  if (status)
    return status;

  /* initialize pseudorandom number generator */
  status = ctr_prng_init(rand_source_get_octet_string);
  if (status)
    return status;

  /* run FIPS-140 statistical tests on ctr_prng */  
  status = stat_test_rand_source_with_repetition(ctr_prng_get_octet_string, MAX_RNG_TRIALS);
  if (status)
    return status;
 
  /* load cipher types */
  status = crypto_kernel_load_cipher_type(&null_cipher, NULL_CIPHER);
  if (status) 
    return status;
  status = crypto_kernel_load_cipher_type(&aes_icm, AES_ICM);
  if (status) 
    return status;
  status = crypto_kernel_load_cipher_type(&aes_cbc, AES_CBC);
  if (status) 
    return status;

  /* load auth func types */
  status = crypto_kernel_load_auth_type(&null_auth, NULL_AUTH);
  if (status)
    return status;
  status = crypto_kernel_load_auth_type(&hmac, HMAC_SHA1);
  if (status)
    return status;

  /* change state to secure */
  crypto_kernel.state = crypto_kernel_state_secure;

  return err_status_ok;
}

err_status_t
crypto_kernel_status() {
  err_status_t status;
  kernel_cipher_type_t  *ctype = crypto_kernel.cipher_type_list;
  kernel_auth_type_t    *atype = crypto_kernel.auth_type_list;
  kernel_debug_module_t *dm    = crypto_kernel.debug_module_list;

  /* run FIPS-140 statistical tests on rand_source */  
  printf("testing rand_source...");
  status = stat_test_rand_source_with_repetition(rand_source_get_octet_string, MAX_RNG_TRIALS);
  if (status) {
    printf("failed\n");
    crypto_kernel.state = crypto_kernel_state_insecure;
    return status;
  }  
  printf("passed\n");

  /* for each cipher type, describe and test */
  while(ctype != NULL) {
    printf("cipher: %s\n", ctype->cipher_type->description);
    printf("  instance count: %d\n", ctype->cipher_type->ref_count);
    printf("  self-test: ");
    status = cipher_type_self_test(ctype->cipher_type);
    if (status) {
      printf("failed with error code %d\n", status);
      exit(status);
    }
    printf("passed\n");
    ctype = ctype->next;
  }
  
  /* for each auth type, describe and test */
  while(atype != NULL) {
    printf("auth func: %s\n", atype->auth_type->description);
    printf("  instance count: %d\n", atype->auth_type->ref_count);
    printf("  self-test: ");
    status = auth_type_self_test(atype->auth_type);
    if (status) {
      printf("failed with error code %d\n", status);
      exit(status);
    }
    printf("passed\n");
    atype = atype->next;
  }

  /* describe each debug module */
  printf("debug modules loaded:\n");
  while (dm != NULL) {
    printf("  %s ", dm->mod->name);  
    if (dm->mod->on)
      printf("(on)\n");
    else
      printf("(off)\n");
    dm = dm->next;
  }

  return err_status_ok;
}

err_status_t
crypto_kernel_list_debug_modules() {
  kernel_debug_module_t *dm = crypto_kernel.debug_module_list;

  /* describe each debug module */
  printf("debug modules loaded:\n");
  while (dm != NULL) {
    printf("  %s ", dm->mod->name);  
    if (dm->mod->on)
      printf("(on)\n");
    else
      printf("(off)\n");
    dm = dm->next;
  }

  return err_status_ok;
}

err_status_t
crypto_kernel_shutdown() {
  err_status_t status;

  /*
   * free dynamic memory used in crypto_kernel at present
   */

  /* walk down cipher type list, freeing memory */
  while (crypto_kernel.cipher_type_list != NULL) {
    kernel_cipher_type_t *ctype = crypto_kernel.cipher_type_list;
    crypto_kernel.cipher_type_list = ctype->next;
    debug_print(mod_crypto_kernel, 
		"freeing memory for cipher %s", 
		ctype->cipher_type->description);
    crypto_free(ctype);
  }

  /* walk down authetication module list, freeing memory */
  while (crypto_kernel.auth_type_list != NULL) {
     kernel_auth_type_t *atype = crypto_kernel.auth_type_list;
     crypto_kernel.auth_type_list = atype->next;
     debug_print(mod_crypto_kernel, 
		"freeing memory for authentication %s",
		atype->auth_type->description);
     crypto_free(atype);
  }

  /* walk down debug module list, freeing memory */
  while (crypto_kernel.debug_module_list != NULL) {
    kernel_debug_module_t *kdm = crypto_kernel.debug_module_list;
    crypto_kernel.debug_module_list = kdm->next;
    debug_print(mod_crypto_kernel, 
		"freeing memory for debug module %s", 
		kdm->mod->name);
    crypto_free(kdm);
  }

  /* de-initialize random number generator */  status = rand_source_deinit();
  if (status)
    return status;

  /* return to insecure state */
  crypto_kernel.state = crypto_kernel_state_insecure;
  
  return err_status_ok;
}

static inline err_status_t
crypto_kernel_do_load_cipher_type(cipher_type_t *new_ct, cipher_type_id_t id,
				  int replace) {
  kernel_cipher_type_t *ctype, *new_ctype;
  err_status_t status;

  /* defensive coding */
  if (new_ct == NULL)
    return err_status_bad_param;

  if (new_ct->id != id)
    return err_status_bad_param;

  /* check cipher type by running self-test */
  status = cipher_type_self_test(new_ct);
  if (status) {
    return status;
  }

  /* walk down list, checking if this type is in the list already  */
  ctype = crypto_kernel.cipher_type_list;
  while (ctype != NULL) {
    if (id == ctype->id) {
      if (!replace)
	return err_status_bad_param;
      status = cipher_type_test(new_ct, ctype->cipher_type->test_data);
      if (status)
	return status;
      new_ctype = ctype;
      break;
    }
    else if (new_ct == ctype->cipher_type)
      return err_status_bad_param;    
    ctype = ctype->next;
  }

  /* if not found, put new_ct at the head of the list */
  if (ctype == NULL) {
  /* allocate memory */
    new_ctype = (kernel_cipher_type_t *) crypto_alloc(sizeof(kernel_cipher_type_t));
    if (new_ctype == NULL)
      return err_status_alloc_fail;
    new_ctype->next = crypto_kernel.cipher_type_list;

    /* set head of list to new cipher type */
    crypto_kernel.cipher_type_list = new_ctype;    
  }
    
  /* set fields */
  new_ctype->cipher_type = new_ct;
  new_ctype->id = id;

  /* load debug module, if there is one present */
  if (new_ct->debug != NULL)
    crypto_kernel_load_debug_module(new_ct->debug);
  /* we could check for errors here */

  return err_status_ok;
}

err_status_t
crypto_kernel_load_cipher_type(cipher_type_t *new_ct, cipher_type_id_t id) {
  return crypto_kernel_do_load_cipher_type(new_ct, id, 0);
}

err_status_t
crypto_kernel_replace_cipher_type(cipher_type_t *new_ct, cipher_type_id_t id) {
  return crypto_kernel_do_load_cipher_type(new_ct, id, 1);
}

err_status_t
crypto_kernel_do_load_auth_type(auth_type_t *new_at, auth_type_id_t id,
				int replace) {
  kernel_auth_type_t *atype, *new_atype;
  err_status_t status;

  /* defensive coding */
  if (new_at == NULL)
    return err_status_bad_param;

  if (new_at->id != id)
    return err_status_bad_param;

  /* check auth type by running self-test */
  status = auth_type_self_test(new_at);
  if (status) {
    return status;
  }

  /* walk down list, checking if this type is in the list already  */
  atype = crypto_kernel.auth_type_list;
  while (atype != NULL) {
    if (id == atype->id) {
      if (!replace)
	return err_status_bad_param;
      status = auth_type_test(new_at, atype->auth_type->test_data);
      if (status)
	return status;
      new_atype = atype;
      break;
    }
    else if (new_at == atype->auth_type)
      return err_status_bad_param;    
    atype = atype->next;
  }

  /* if not found, put new_at at the head of the list */
  if (atype == NULL) {
    /* allocate memory */
    new_atype = (kernel_auth_type_t *)crypto_alloc(sizeof(kernel_auth_type_t));
    if (new_atype == NULL)
      return err_status_alloc_fail;

    new_atype->next = crypto_kernel.auth_type_list;
    /* set head of list to new auth type */
    crypto_kernel.auth_type_list = new_atype;
  }
    
  /* set fields */
  new_atype->auth_type = new_at;
  new_atype->id = id;

  /* load debug module, if there is one present */
  if (new_at->debug != NULL)
    crypto_kernel_load_debug_module(new_at->debug);
  /* we could check for errors here */

  return err_status_ok;

}

err_status_t
crypto_kernel_load_auth_type(auth_type_t *new_at, auth_type_id_t id) {
  return crypto_kernel_do_load_auth_type(new_at, id, 0);
}

err_status_t
crypto_kernel_replace_auth_type(auth_type_t *new_at, auth_type_id_t id) {
  return crypto_kernel_do_load_auth_type(new_at, id, 1);
}


cipher_type_t *
crypto_kernel_get_cipher_type(cipher_type_id_t id) {
  kernel_cipher_type_t *ctype;
  
  /* walk down list, looking for id  */
  ctype = crypto_kernel.cipher_type_list;
  while (ctype != NULL) {
    if (id == ctype->id)
      return ctype->cipher_type; 
    ctype = ctype->next;
  } 

  /* haven't found the right one, indicate failure by returning NULL */
  return NULL;
}


err_status_t
crypto_kernel_alloc_cipher(cipher_type_id_t id, 
			      cipher_pointer_t *cp, 
			      int key_len) {
  cipher_type_t *ct;

  /* 
   * if the crypto_kernel is not yet initialized, we refuse to allocate
   * any ciphers - this is a bit extra-paranoid
   */
  if (crypto_kernel.state != crypto_kernel_state_secure)
    return err_status_init_fail;

  ct = crypto_kernel_get_cipher_type(id);
  if (!ct)
    return err_status_fail;
  
  return ((ct)->alloc(cp, key_len));
}



auth_type_t *
crypto_kernel_get_auth_type(auth_type_id_t id) {
  kernel_auth_type_t *atype;
  
  /* walk down list, looking for id  */
  atype = crypto_kernel.auth_type_list;
  while (atype != NULL) {
    if (id == atype->id)
      return atype->auth_type; 
    atype = atype->next;
  } 

  /* haven't found the right one, indicate failure by returning NULL */
  return NULL;
}

err_status_t
crypto_kernel_alloc_auth(auth_type_id_t id, 
			 auth_pointer_t *ap, 
			 int key_len,
			 int tag_len) {
  auth_type_t *at;

  /* 
   * if the crypto_kernel is not yet initialized, we refuse to allocate
   * any auth functions - this is a bit extra-paranoid
   */
  if (crypto_kernel.state != crypto_kernel_state_secure)
    return err_status_init_fail;

  at = crypto_kernel_get_auth_type(id);
  if (!at)
    return err_status_fail;
  
  return ((at)->alloc(ap, key_len, tag_len));
}

err_status_t
crypto_kernel_load_debug_module(debug_module_t *new_dm) {
  kernel_debug_module_t *kdm, *new;

  /* defensive coding */
  if (new_dm == NULL)
    return err_status_bad_param;

  /* walk down list, checking if this type is in the list already  */
  kdm = crypto_kernel.debug_module_list;
  while (kdm != NULL) {
    if (strncmp(new_dm->name, kdm->mod->name, 64) == 0)
      return err_status_bad_param;    
    kdm = kdm->next;
  }

  /* put new_dm at the head of the list */
  /* allocate memory */
  new = (kernel_debug_module_t *)crypto_alloc(sizeof(kernel_debug_module_t));
  if (new == NULL)
    return err_status_alloc_fail;
    
  /* set fields */
  new->mod = new_dm;
  new->next = crypto_kernel.debug_module_list;

  /* set head of list to new cipher type */
  crypto_kernel.debug_module_list = new;    

  return err_status_ok;
}

err_status_t
crypto_kernel_set_debug_module(char *name, int on) {
  kernel_debug_module_t *kdm;
  
  /* walk down list, checking if this type is in the list already  */
  kdm = crypto_kernel.debug_module_list;
  while (kdm != NULL) {
    if (strncmp(name, kdm->mod->name, 64) == 0) {
      kdm->mod->on = on;
      return err_status_ok;
    }
    kdm = kdm->next;
  }

  return err_status_fail;
}

err_status_t
crypto_get_random(unsigned char *buffer, unsigned int length) {
  if (crypto_kernel.state == crypto_kernel_state_secure)
    return ctr_prng_get_octet_string(buffer, length);
  else
    return err_status_fail;
}
