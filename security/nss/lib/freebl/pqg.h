/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  pqg.h
 *
 *  header file for pqg functions exported just to freebl
 */

#ifndef _PQG_H_
#define _PQG_H_ 1

SECStatus
PQG_HashBuf(HASH_HashType type, unsigned char *dest,
            const unsigned char *src, PRUint32 src_len);
/* PQG_GetLength returns the significant bytes in the SECItem object (that is
 * the length of the object minus any leading zeros. Any SECItem may be used,
 * though this function is usually used for P, Q, or G values */
unsigned int PQG_GetLength(const SECItem *obj);
/* Check to see the PQG parameters patch a NIST defined DSA size,
 * returns SECFaillure and sets SEC_ERROR_INVALID_ARGS if it doesn't.
 * See blapi.h for legal DSA PQG sizes. */
SECStatus PQG_Check(const PQGParams *params);
/* Return the prefered hash algorithm for the given PQGParameters. */
HASH_HashType PQG_GetHashType(const PQGParams *params);

#endif /* _PQG_H_ */
