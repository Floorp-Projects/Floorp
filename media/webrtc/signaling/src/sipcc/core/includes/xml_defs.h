/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _XML_DEFS_H_
#define _XML_DEFS_H_

#include "cpr_types.h"

#define TOK_MAX_LEN   (24)

#define TOK_NONE      (0)
#define TOK_PROC      (1)
#define TOK_ELEMENT   (2)
#define TOK_ATTRIBUTE (3)
#define TOK_CONTENT   (4)

#define XML_START       (1199)
#define XML_END         (1200)
#define XML_CONTENT     (1201)
#define XML_ALL_CONTENT (1202)

typedef struct XMLTokenStruc {
    const int8_t strTok[TOK_MAX_LEN];
    int16_t TokType;
    void *pData; // This may be another XMLTable,
                 // or an array of valid attribute values.
} XMLTokenStruc;

typedef void (*XmlFunc)(int16_t, void *);

typedef struct XMLTableStruc {
    void (*xml_func)( int16_t TokID, void *pData );
    uint8_t nNumTok;
    const XMLTokenStruc *pTok;
} XMLTableStruc;

typedef enum {
    TOK_ERR,
    TOK_EOF,          /* reached end of the input */
    TOK_LBRACKET,     /* "<" */
    TOK_ENDLBRACKET,  /* "</" */
    TOK_EMPTYBRACKET, /* "/>" */
    TOK_RBRACKET,     /* ">" */
    TOK_EQ,           /* "=" */
    TOK_STR,          /* string on the rhs of = */
    TOK_KEYWORD,      /* string on lhs of = */
    TOK_CONTENT_STR
} XMLToken;

#endif
