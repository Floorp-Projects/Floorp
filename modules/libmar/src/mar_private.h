/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MAR_PRIVATE_H__
#define MAR_PRIVATE_H__

#include "limits.h"
#include "mozilla/Assertions.h"
#include <stdint.h>

#define BLOCKSIZE 4096
#define ROUND_UP(n, incr) (((n) / (incr) + 1) * (incr))

#define MAR_ID "MAR1"
#define MAR_ID_SIZE 4

/* The signature block comes directly after the header block 
   which is 16 bytes */
#define SIGNATURE_BLOCK_OFFSET 16

/* Make sure the file is less than 500MB.  We do this to protect against
   invalid MAR files. */
#define MAX_SIZE_OF_MAR_FILE ((int64_t)524288000)

/* Existing code makes assumptions that the file size is
   smaller than LONG_MAX. */
MOZ_STATIC_ASSERT(MAX_SIZE_OF_MAR_FILE < ((int64_t)LONG_MAX),
                  "max mar file size is too big");

/* We store at most the size up to the signature block + 4 
   bytes per BLOCKSIZE bytes */
MOZ_STATIC_ASSERT(sizeof(BLOCKSIZE) < \
                  (SIGNATURE_BLOCK_OFFSET + sizeof(uint32_t)),
                  "BLOCKSIZE is too big");

/* The maximum size of any signature supported by current and future
   implementations of the signmar program. */
#define MAX_SIGNATURE_LENGTH 2048

/* Each additional block has a unique ID.  
   The product information block has an ID of 1. */
#define PRODUCT_INFO_BLOCK_ID 1

#define MAR_ITEM_SIZE(namelen) (3*sizeof(uint32_t) + (namelen) + 1)

/* Product Information Block (PIB) constants */
#define PIB_MAX_MAR_CHANNEL_ID_SIZE 63
#define PIB_MAX_PRODUCT_VERSION_SIZE 31

/* The mar program is compiled as a host bin so we don't have access to NSPR at
   runtime.  For that reason we use ntohl, htonl, and define HOST_TO_NETWORK64 
   instead of the NSPR equivalents. */
#ifdef XP_WIN
#include <winsock2.h>
#define ftello _ftelli64
#define fseeko _fseeki64
#else
#define _FILE_OFFSET_BITS 64
#include <netinet/in.h>
#include <unistd.h>
#endif

#include <stdio.h>

#define HOST_TO_NETWORK64(x) ( \
  ((((uint64_t) x) & 0xFF) << 56) | \
  ((((uint64_t) x) >> 8) & 0xFF) << 48) | \
  (((((uint64_t) x) >> 16) & 0xFF) << 40) | \
  (((((uint64_t) x) >> 24) & 0xFF) << 32) | \
  (((((uint64_t) x) >> 32) & 0xFF) << 24) | \
  (((((uint64_t) x) >> 40) & 0xFF) << 16) | \
  (((((uint64_t) x) >> 48) & 0xFF) << 8) | \
  (((uint64_t) x) >> 56)
#define NETWORK_TO_HOST64 HOST_TO_NETWORK64

#endif  /* MAR_PRIVATE_H__ */
