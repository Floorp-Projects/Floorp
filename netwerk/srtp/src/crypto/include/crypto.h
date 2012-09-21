/*
 * crypto.h
 *
 * API for libcrypto
 * 
 * David A. McGrew
 * Cisco Systems, Inc.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

/** 
 *  @brief A cipher_type_id_t is an identifier for a particular cipher
 *  type.
 *
 *  A cipher_type_id_t is an integer that represents a particular
 *  cipher type, e.g. the Advanced Encryption Standard (AES).  A
 *  NULL_CIPHER is avaliable; this cipher leaves the data unchanged,
 *  and can be selected to indicate that no encryption is to take
 *  place.
 * 
 *  @ingroup Ciphers
 */
typedef uint32_t cipher_type_id_t; 

/**
 *  @brief An auth_type_id_t is an identifier for a particular authentication
 *   function.
 *
 *  An auth_type_id_t is an integer that represents a particular
 *  authentication function type, e.g. HMAC-SHA1.  A NULL_AUTH is
 *  avaliable; this authentication function performs no computation,
 *  and can be selected to indicate that no authentication is to take
 *  place.
 *  
 *  @ingroup Authentication
 */
typedef uint32_t auth_type_id_t;

#endif /* CRYPTO_H */


