/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ssl3encode_h_
#define __ssl3encode_h_

#include "seccomon.h"

/* All of these functions modify the underlying SECItem, and so should
 * be performed on a shallow copy.*/
SECStatus ssl3_AppendToItem(SECItem *item,
                            const unsigned char *buf, PRUint32 bytes);
SECStatus ssl3_AppendNumberToItem(SECItem *item,
                                  PRUint32 num, PRInt32 lenSize);
SECStatus ssl3_ConsumeFromItem(SECItem *item,
                               unsigned char **buf, PRUint32 bytes);
SECStatus ssl3_ConsumeNumberFromItem(SECItem *item,
                                     PRUint32 *num, PRUint32 bytes);
PRUint8 *ssl_EncodeUintX(PRUint64 value, unsigned int bytes, PRUint8 *to);

#endif
