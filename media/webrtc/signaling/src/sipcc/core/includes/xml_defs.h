/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
