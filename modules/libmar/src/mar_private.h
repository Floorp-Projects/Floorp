/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MAR_PRIVATE_H__
#define MAR_PRIVATE_H__

#include "prtypes.h"
#include "limits.h"

/* Code in this module requires a guarantee that the size
   of PRUint32 and PRUint64 are 4 and 8 bytes respectively. */
PR_STATIC_ASSERT(sizeof(PRUint32) == 4);
PR_STATIC_ASSERT(sizeof(PRUint64) == 8);

#define BLOCKSIZE 4096
#define ROUND_UP(n, incr) (((n) / (incr) + 1) * (incr))

#define MAR_ID "MAR1"
#define MAR_ID_SIZE 4

/* The signature block comes directly after the header block 
   which is 16 bytes */
#define SIGNATURE_BLOCK_OFFSET 16

/* We have a MAX_SIGNATURES limit so that an invalid MAR will never
   waste too much of either updater's or signmar's time. */
#define MAX_SIGNATURES 8

/* Make sure the file is less than 500MB.  We do this to protect against
   invalid MAR files. */
#define MAX_SIZE_OF_MAR_FILE ((PRInt64)524288000)

/* Existing code makes assumptions that the file size is
   smaller than LONG_MAX. */
PR_STATIC_ASSERT(MAX_SIZE_OF_MAR_FILE < ((PRInt64)LONG_MAX));

/* We store at most the size up to the signature block + 4 
   bytes per BLOCKSIZE bytes */
PR_STATIC_ASSERT(sizeof(BLOCKSIZE) < \
  (SIGNATURE_BLOCK_OFFSET + sizeof(PRUint32)));

/* The maximum size of any signature supported by current and future
   implementations of the signmar program. */
#define MAX_SIGNATURE_LENGTH 2048

/* Each additional block has a unique ID.  
   The product information block has an ID of 1. */
#define PRODUCT_INFO_BLOCK_ID 1

#define MAR_ITEM_SIZE(namelen) (3*sizeof(PRUint32) + (namelen) + 1)

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
  ((((PRUint64) x) & 0xFF) << 56) | \
  ((((PRUint64) x) >> 8) & 0xFF) << 48) | \
  (((((PRUint64) x) >> 16) & 0xFF) << 40) | \
  (((((PRUint64) x) >> 24) & 0xFF) << 32) | \
  (((((PRUint64) x) >> 32) & 0xFF) << 24) | \
  (((((PRUint64) x) >> 40) & 0xFF) << 16) | \
  (((((PRUint64) x) >> 48) & 0xFF) << 8) | \
  (((PRUint64) x) >> 56)
#define NETWORK_TO_HOST64 HOST_TO_NETWORK64

#endif  /* MAR_PRIVATE_H__ */
