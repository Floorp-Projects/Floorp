/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is PRIVATE to SSL.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslencode_h_
#define __sslencode_h_

/* A buffer object, used for assembling messages. */
typedef struct sslBufferStr {
    PRUint8 *buf;
    unsigned int len;
    unsigned int space;
    /* Set to true if the storage for the buffer is fixed, such as a stack
     * variable or a view on another buffer. Growing a fixed buffer fails. */
    PRBool fixed;
} sslBuffer;

#define SSL_BUFFER_EMPTY     \
    {                        \
        NULL, 0, 0, PR_FALSE \
    }
#define SSL_BUFFER_FIXED(b, maxlen) \
    {                               \
        b, 0, maxlen, PR_TRUE       \
    }
#define SSL_BUFFER_FIXED_LEN(b, len) \
    {                                \
        b, len, 0, PR_TRUE           \
    }
#define SSL_BUFFER(b) SSL_BUFFER_FIXED(b, sizeof(b))
#define SSL_BUFFER_BASE(b) ((b)->buf)
#define SSL_BUFFER_LEN(b) ((b)->len)
#define SSL_BUFFER_NEXT(b) ((b)->buf + (b)->len)
#define SSL_BUFFER_SPACE(b) ((b)->space - (b)->len)

SECStatus sslBuffer_Grow(sslBuffer *b, unsigned int newLen);
SECStatus sslBuffer_Append(sslBuffer *b, const void *data, unsigned int len);
SECStatus sslBuffer_AppendNumber(sslBuffer *b, PRUint64 v, unsigned int size);
SECStatus sslBuffer_AppendVariable(sslBuffer *b, const PRUint8 *data,
                                   unsigned int len, unsigned int size);
SECStatus sslBuffer_AppendBuffer(sslBuffer *b, const sslBuffer *append);
SECStatus sslBuffer_AppendBufferVariable(sslBuffer *b, const sslBuffer *append,
                                         unsigned int size);
SECStatus sslBuffer_Skip(sslBuffer *b, unsigned int size,
                         unsigned int *savedOffset);
SECStatus sslBuffer_InsertLength(sslBuffer *b, unsigned int at,
                                 unsigned int size);
SECStatus sslBuffer_InsertNumber(sslBuffer *b, unsigned int at,
                                 PRUint64 v, unsigned int size);
void sslBuffer_Clear(sslBuffer *b);

SECStatus ssl3_AppendHandshake(sslSocket *ss, const void *void_src,
                               unsigned int bytes);
SECStatus ssl3_AppendHandshakeSuppressHash(sslSocket *ss, const void *void_src,
                                           unsigned int bytes);
SECStatus ssl3_AppendHandshakeHeader(sslSocket *ss,
                                     SSLHandshakeType t, unsigned int length);
SECStatus ssl3_AppendHandshakeNumber(sslSocket *ss, PRUint64 num,
                                     unsigned int lenSize);
SECStatus ssl3_AppendHandshakeVariable(sslSocket *ss, const PRUint8 *src,
                                       unsigned int bytes, unsigned int lenSize);
SECStatus ssl3_AppendBufferToHandshake(sslSocket *ss, sslBuffer *buf);
SECStatus ssl3_AppendBufferToHandshakeVariable(sslSocket *ss, sslBuffer *buf,
                                               unsigned int lenSize);

typedef struct {
    const PRUint8 *buf;
    unsigned int len;
} sslReadBuffer;
typedef struct {
    sslReadBuffer buf;
    unsigned int offset;
} sslReader;
#define SSL_READER(b, l) \
    {                    \
        { b, l }, 0      \
    }
#define SSL_READER_CURRENT(r) \
    ((r)->buf.buf + (r)->offset)
#define SSL_READER_REMAINING(r) \
    ((r)->buf.len - (r)->offset)
SECStatus sslRead_Read(sslReader *reader, unsigned int count,
                       sslReadBuffer *out);
SECStatus sslRead_ReadVariable(sslReader *reader, unsigned int sizeLen,
                               sslReadBuffer *out);
SECStatus sslRead_ReadNumber(sslReader *reader, unsigned int bytes,
                             PRUint64 *val);

#endif /* __sslencode_h_ */
