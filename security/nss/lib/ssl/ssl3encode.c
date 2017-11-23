/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prnetdb.h"
#include "seccomon.h"
#include "secerr.h"
#include "ssl3encode.h"

SECStatus
ssl3_AppendToItem(SECItem *item, const unsigned char *buf, PRUint32 bytes)
{
    if (bytes > item->len) {
        PORT_SetError(SEC_ERROR_INVALID_ARGS);
        return SECFailure;
    }

    PORT_Memcpy(item->data, buf, bytes);
    item->data += bytes;
    item->len -= bytes;
    return SECSuccess;
}

SECStatus
ssl3_AppendNumberToItem(SECItem *item, PRUint32 num, PRInt32 lenSize)
{
    SECStatus rv;
    PRUint8 b[4];

    ssl_EncodeUintX(num, lenSize, b);
    rv = ssl3_AppendToItem(item, &b[0], lenSize);
    return rv;
}

SECStatus
ssl3_ConsumeFromItem(SECItem *item, unsigned char **buf, PRUint32 bytes)
{
    if (bytes > item->len) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    *buf = item->data;
    item->data += bytes;
    item->len -= bytes;
    return SECSuccess;
}

SECStatus
ssl3_ConsumeNumberFromItem(SECItem *item, PRUint32 *num, PRUint32 bytes)
{
    int i;

    if (bytes > item->len || bytes > sizeof(*num)) {
        PORT_SetError(SEC_ERROR_BAD_DATA);
        return SECFailure;
    }

    *num = 0;
    for (i = 0; i < bytes; i++) {
        *num = (*num << 8) + item->data[i];
    }

    item->data += bytes;
    item->len -= bytes;

    return SECSuccess;
}

/* Helper function to encode an unsigned integer into a buffer. */
PRUint8 *
ssl_EncodeUintX(PRUint64 value, unsigned int bytes, PRUint8 *to)
{
    PRUint64 encoded;

    PORT_Assert(bytes > 0 && bytes <= sizeof(encoded));

    encoded = PR_htonll(value);
    memcpy(to, ((unsigned char *)(&encoded)) + (sizeof(encoded) - bytes), bytes);
    return to + bytes;
}
