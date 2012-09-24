/*
 * crypto.h
 *
 * API for libcrypto
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


