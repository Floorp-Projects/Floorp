/*
 * xfm.h
 *
 * interface for abstract crypto transform
 * 
 * David A. McGrew
 * Cisco Systems, Inc.
 */

#ifndef XFM_H
#define XFM_H

#include "crypto_kernel.h"
#include "err.h"

/**
 * @defgroup Crypto Cryptography
 *
 * A simple interface to an abstract cryptographic transform that
 * provides both confidentiality and message authentication.
 *
 * @{
 */

/**
 * @brief applies a crypto transform
 *
 * The function pointer xfm_func_t points to a function that
 * implements a crypto transform, and provides a uniform API for
 * accessing crypto mechanisms.
 * 
 * @param key       location of secret key                  
 *
 * @param clear     data to be authenticated only           
 *
 * @param clear_len length of data to be authenticated only 
 *
 * @param iv        location to write the Initialization Vector (IV)
 *
 * @param protect   location of the data to be encrypted and
 * authenticated (before the function call), and the ciphertext
 * and authentication tag (after the call)
 *
 * @param protected_len location of the length of the data to be
 * encrypted and authenticated (before the function call), and the
 * length of the ciphertext (after the call)
 *
 * @param auth_tag   location to write auth tag              
 */

typedef err_status_t (*xfm_func_t) 
     (void *key,            
      void *clear,          
      unsigned clear_len,   
      void *iv,             
      void *protect,         
      unsigned *protected_len, 
      void *auth_tag        
      );

typedef 
err_status_t (*xfm_inv_t)
     (void *key,            /* location of secret key                  */
      void *clear,          /* data to be authenticated only           */
      unsigned clear_len,   /* length of data to be authenticated only */
      void *iv,             /* location of iv                          */
      void *opaque,         /* data to be decrypted and authenticated  */
      unsigned *opaque_len, /* location of the length of data to be
			     * decrypted and authd (before and after) 
			     */
      void *auth_tag        /* location of auth tag                    */
      );

typedef struct xfm_ctx_t {
  xfm_func_t func;
  xfm_inv_t  inv;
  unsigned key_len;
  unsigned iv_len;
  unsigned auth_tag_len;
} xfm_ctx_t;

typedef xfm_ctx_t *xfm_t;

#define xfm_get_key_len(xfm) ((xfm)->key_len)

#define xfm_get_iv_len(xfm) ((xfm)->iv_len)

#define xfm_get_auth_tag_len(xfm) ((xfm)->auth_tag_len)


/* cryptoalgo - 5/28 */
  
typedef err_status_t (*cryptoalg_func_t) 
     (void *key,            
      void *clear,          
      unsigned clear_len,   
      void *iv,             
      void *opaque,         
      unsigned *opaque_len
      );

typedef 
err_status_t (*cryptoalg_inv_t)
     (void *key,            /* location of secret key                  */
      void *clear,          /* data to be authenticated only           */
      unsigned clear_len,   /* length of data to be authenticated only */
      void *iv,             /* location of iv                          */
      void *opaque,         /* data to be decrypted and authenticated  */
      unsigned *opaque_len  /* location of the length of data to be
			     * decrypted and authd (before and after) 
			     */
      );

typedef struct cryptoalg_ctx_t {
  cryptoalg_func_t enc;
  cryptoalg_inv_t  dec;
  unsigned key_len;
  unsigned iv_len;
  unsigned auth_tag_len;
  unsigned max_expansion; 
} cryptoalg_ctx_t;

typedef cryptoalg_ctx_t *cryptoalg_t;

#define cryptoalg_get_key_len(cryptoalg) ((cryptoalg)->key_len)

#define cryptoalg_get_iv_len(cryptoalg) ((cryptoalg)->iv_len)

#define cryptoalg_get_auth_tag_len(cryptoalg) ((cryptoalg)->auth_tag_len)



/**
 * @}
 */

#endif /* XFM_H */


